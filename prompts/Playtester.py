import subprocess
import threading
import queue
import time
import re
import json
import os
from datetime import datetime
from typing import Literal
from pydantic import BaseModel, Field

import google.generativeai as genai
from openai import OpenAI

GAME_PATH = "/app/backend/data/archeron-9.z5"
FROTZ_BIN = "/usr/games/dfrotz"
LOG_DIR = "/app/backend/data"
STUCK_LIMIT = 6


def clean_json_response(text: str) -> str:
    return re.sub(
        r"^```json[\s]*|^```[\s]*|[\s]*```$", "", text, flags=re.MULTILINE
    ).strip()


class FrotzProcess:
    _PAUSE_RE = re.compile(
        r"(\[more\]|\[press any key\]|press any key to continue|press a key|press enter to continue|\(more\)|press any key to begin)",
        re.IGNORECASE,
    )

    def __init__(self, game_path: str, timeout: float = 10.0):
        self.timeout = timeout
        self._q: queue.Queue[str] = queue.Queue()
        self._proc = subprocess.Popen(
            [FROTZ_BIN, "-w", "250", "-h", "100", "-m", game_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()
        self._handle_initial_prompt()

    def _reader(self) -> None:
        try:
            assert self._proc.stdout
            for line in self._proc.stdout:
                self._q.put(line)
        except Exception:
            pass

    def _send_raw(self, text: str) -> None:
        assert self._proc.stdin
        self._proc.stdin.write(text)
        self._proc.stdin.flush()

    def _handle_initial_prompt(self) -> None:
        time.sleep(1.0)
        deadline = time.time() + 3.0
        while time.time() < deadline:
            try:
                line = self._q.get(timeout=0.2)
                if self._PAUSE_RE.search(line):
                    self._send_raw("\n")
                    deadline = time.time() + 1.0
            except queue.Empty:
                break

    def read_output(self) -> str:
        chunks = []
        deadline = time.time() + self.timeout
        while time.time() < deadline:
            try:
                line = self._q.get(timeout=0.3)
                if self._PAUSE_RE.search(line):
                    self._send_raw("\n")
                    line = self._PAUSE_RE.sub("", line)
                    deadline = time.time() + self.timeout
                else:
                    chunks.append(line)
                    deadline = time.time() + 0.8
            except queue.Empty:
                if chunks:
                    break

        raw = "".join(chunks)
        text = re.sub(r"\x1b\[[0-9;]*[mGKHF]", "", raw)
        text = re.sub(r"(?m)^>.*\n?", "", text)
        text = re.sub(r"(?<!\n)\n(?!\n)", " ", text)
        text = re.sub(r" +", " ", text)
        return re.sub(r"\s*>\s*$", "", text.strip()).strip()

    def send_command(self, cmd: str) -> str:
        while not self._q.empty():
            try:
                self._q.get_nowait()
            except queue.Empty:
                break
        self._send_raw(cmd.strip() + "\n")
        return self.read_output()

    def close(self):
        try:
            self._send_raw("QUIT\nY\n")
        except Exception:
            pass
        try:
            self._proc.terminate()
            self._proc.wait(timeout=3)
        except Exception:
            pass


def run_playtester(valves, max_turns: int):
    genai.configure(api_key=valves.GEMINI_API_KEY)
    gemini_model = genai.GenerativeModel("gemini-2.5-flash-lite")
    deepseek_client = OpenAI(
        api_key=valves.DEEPSEEK_API_KEY,
        base_url="https://api.deepseek.com/v1",
    )

    frotz = FrotzProcess(GAME_PATH)
    deepseek_history = []
    transcript = []
    last_location_content = ""
    last_command = ""
    stuck_count = 0
    game_memory = ""
    _ALARM_RE = re.compile(
        r"red alarm lights.*?booms!.*?$", re.IGNORECASE | re.MULTILINE
    )

    try:
        raw_output = frotz.read_output()
        transcript.append(f"# GAME START\n\n{raw_output}\n")

        for turn in range(1, max_turns + 1):
            reporter_resp = gemini_model.generate_content(
                f"RAW TEXT:\n{raw_output}\n\n{valves.PROMPT_REPORTER}"
            )
            try:
                scene_json = json.loads(clean_json_response(reporter_resp.text))
            except Exception:
                scene_json = {
                    "location": "Unknown",
                    "description": raw_output,
                    "objects": [],
                    "exits": [],
                }

            # Update the persistent memory summary
            memory_input = (
                f"CURRENT MEMORY:\n{game_memory}\n\n"
                f"LAST COMMAND: {last_command}\n"
                f"CURRENT SCENE:\n{json.dumps(scene_json)}"
            )
            memory_resp = gemini_model.generate_content(
                f"{valves.PROMPT_MEMORY}\n\n{memory_input}"
            )
            game_memory = memory_resp.text.strip()

            # Strip alarm boilerplate so repeating sirens don't mask real stuck state
            raw_description = scene_json.get("description", "")
            clean_description = _ALARM_RE.sub("", raw_description).strip()
            current_content = (
                f"{scene_json.get('location')} {clean_description}".lower()
            )
            if current_content == last_location_content:
                stuck_count += 1
            else:
                stuck_count = 0
            last_location_content = current_content

            if stuck_count >= STUCK_LIMIT:
                transcript.append(f"\n**[STUCK]** Halting at turn {turn}.")
                break

            strategist_prompt = valves.PROMPT_STRATEGIST
            if valves.HINTS.strip():
                if valves.HINT_MODE == "override":
                    hint_block = f"\n\nCRITICAL HINTS — follow these before all other priorities:\n{valves.HINTS.strip()}"
                else:
                    hint_block = f"\n\nAdvisory hints — weigh these alongside your priorities:\n{valves.HINTS.strip()}"
                strategist_prompt += hint_block

            messages = [{"role": "system", "content": strategist_prompt}]
            messages.extend(deepseek_history[-10:])
            messages.append({
                "role": "user",
                "content": f"GAME MEMORY:\n{game_memory}\n\nCURRENT SCENE:\n{json.dumps(scene_json)}",
            })

            try:
                strat_comp = deepseek_client.chat.completions.create(
                    model=valves.PLANNER_MODEL, messages=messages
                )
                strategy_text = strat_comp.choices[0].message.content
            except Exception as e:
                strategy_text = f"REASONING: API Error ({str(e)}). INTENT: Look."

            deepseek_history.append({"role": "user", "content": json.dumps(scene_json)})
            deepseek_history.append({"role": "assistant", "content": strategy_text})

            intent = "Look"
            intent_match = re.search(r"INTENT:\s*(.*)", strategy_text, re.IGNORECASE)
            if intent_match:
                intent = intent_match.group(1).strip()

            encoder_input = (
                f"SCENE OBJECTS: {scene_json.get('objects')}\n"
                f"GAME MESSAGES: {scene_json.get('game_messages', [])}\n"
                f"DESIRED ACTION: {intent}"
            )
            encoder_resp = gemini_model.generate_content(
                f"{valves.PROMPT_ENCODER}\n\n{encoder_input}"
            )

            command = "LOOK"
            cmd_match = re.search(r"COMMAND:\s*(.*)", encoder_resp.text, re.IGNORECASE)
            if cmd_match:
                command = cmd_match.group(1).strip().upper()
                command = " ".join(
                    [w for w in command.split() if w not in ["THE", "A", "AN"]]
                )

            # Whitelist check: first token must be a known IF verb
            _VALID_VERBS = {
                "N",
                "S",
                "E",
                "W",
                "NE",
                "NW",
                "SE",
                "SW",
                "U",
                "D",
                "X",
                "LOOK",
                "I",
                "GET",
                "DROP",
                "WEAR",
                "OPEN",
                "CLOSE",
                "UNLOCK",
                "INSERT",
                "PUT",
                "SEARCH",
                "PUSH",
                "PULL",
                "TURN",
                "TAKE",
                "REMOVE",
                "READ",
            }
            first_token = command.split()[0] if command.split() else ""
            if first_token not in _VALID_VERBS:
                transcript.append(
                    f"  [Encoder produced invalid verb '{first_token}' — falling back to LOOK]\n"
                )
                command = "LOOK"

            # If the encoder produced an identical command to last turn, force LOOK to break the loop
            if command == last_command:
                command = "LOOK"
            last_command = command

            transcript.append(
                f"\n--- TURN {turn} ---\n**STRATEGY**: {strategy_text}\n**COMMAND**: `> {command}`\n"
            )
            raw_output = frotz.send_command(command)
            transcript.append(f"{raw_output}\n")

            if any(
                m in raw_output.lower()
                for m in ["*** you have died", "win", "the end", "restart", "restore"]
            ):
                transcript.append("\n**GAME OVER**")
                break
    finally:
        frotz.close()

    ts = datetime.now().strftime("%H%M%S")
    log_path = os.path.join(LOG_DIR, f"playtest_{ts}.txt")
    with open(log_path, "w") as f:
        f.write("".join(transcript))

    return f"Playtest Complete. Result saved to {log_path}"


class Tools:
    """Autonomous IF Playtester — all prompts exposed as Valves."""

    class Valves(BaseModel):
        GEMINI_API_KEY: str = Field(default="", description="Gemini API Key")
        DEEPSEEK_API_KEY: str = Field(default="", description="DeepSeek API Key")
        PLANNER_MODEL: Literal["deepseek-chat", "deepseek-reasoner"] = Field(
            default="deepseek-chat"
        )
        HINTS: str = Field(
            default="",
            description=(
                "Optional hints for the Strategist. Use plain text or a numbered list. "
                "Example: '1. The card goes in the slot by the door. 2. You need the suit before going outside.'"
            ),
        )
        HINT_MODE: Literal["suggest", "override"] = Field(
            default="suggest",
            description=(
                "How hints are applied. "
                "'suggest' adds them as advisory context the Strategist can weigh alongside other priorities. "
                "'override' instructs the Strategist to follow them before all other priorities."
            ),
        )

        PROMPT_REPORTER: str = Field(
            default=(
                "You are a parser for a Z-machine text adventure. "
                "Convert the raw game output below into a JSON object. "
                "Do not invent, infer, or add anything not explicitly stated in the text.\n\n"
                "Fields:\n"
                '- location: the room name from the status bar or first line. Use "Unknown" only if truly absent.\n'
                "- description: the cleaned game prose. Remove ANSI codes, status bars, [More] prompts, "
                'and alarm boilerplate (lines containing "alarm lights" or "siren"). '
                "Keep all other narrative text verbatim.\n"
                "- objects: list every physical noun the player can interact with, exactly as named in the text. "
                'Examples from this game: "grey jumpsuit", "dark cylinder", "plastic card", '
                '"desiccated corpse", "pods", "mechanism", "column". '
                "If the text mentions any object, it must appear here. Never return an empty list if objects are present.\n"
                "- exits: list every direction or passage explicitly mentioned. "
                'Examples: "north", "east", "west", "archway west".\n'
                '- game_messages: any parser feedback lines such as "I beg your pardon", '
                '"That\'s not a verb I recognise", "You can\'t see any such thing", '
                '"You can\'t carry that", "The door is locked". Copy them verbatim.\n\n'
                "Output ONLY raw JSON. No markdown fences, no preamble, no trailing text.\n"
                'Schema: {"location":"","description":"",'
                '"objects":[],"exits":[],"game_messages":[]}'
            ),
            description="Prompt for the Gemini Reporter agent",
        )

        PROMPT_MEMORY: str = Field(
            default=(
                "You are the memory system for an autonomous text adventure player. "
                "Your job is to maintain a concise, accurate summary of everything learned so far.\n\n"
                "You will receive the current memory, the last command issued, and the current scene.\n"
                "Update the memory to reflect any new information from the current scene.\n\n"
                "Track the following:\n"
                "- ROOMS VISITED: Each room name and its key features and exits.\n"
                "- INVENTORY: Items currently held by the player.\n"
                "- FAILED ACTIONS: Commands that produced no result or an error, so they are not repeated.\n"
                "- ESTABLISHED FACTS: Confirmed interactions, puzzle elements, and cause-effect relationships observed.\n"
                "- LEADING HYPOTHESIS: The most promising next step based on everything known.\n\n"
                "Rules:\n"
                "- Be concise. Use short bullet points. Do not pad or repeat.\n"
                "- Never invent information not present in the scene or prior memory.\n"
                "- If the current memory is empty, start fresh from the current scene.\n"
                "- Output only the updated memory text. No preamble, no explanation."
            ),
            description="Prompt for the Gemini Memory agent",
        )

        PROMPT_STRATEGIST: str = Field(
            default=(
                "You are playing a sci-fi text adventure. Your goal is to solve puzzles and explore.\n"
                "You will receive a GAME MEMORY summary of everything learned so far, followed by the CURRENT SCENE.\n"
                "Use the memory to avoid repeating failed actions and to reason across turns.\n\n"
                "CORE DIRECTIVES:\n"
                "1. INTERACT: If a new object is visible (cylinder, card, corpse), try to TAKE or EXAMINE it.\n"
                "2. EXPERIMENT: If you have an item and a logical target exists (card/slot, cylinder/port), try to combine them.\n"
                "3. NAVIGATE: If a room is empty or a door is open, move to a new direction (N, S, E, W).\n"
                "4. EXPLORE:If you have examined all visible objects in a room and have no new actions available, you MUST move to an unexplored exit. Do not stay in a room you have fully explored.\n\n"
                "STRATEGY RULES:\n"
                "- If you are 'Encumbered', your hands are full. DROP an item you've already used.\n"
                "- If an item is 'integrated' or 'biological', stop trying to TAKE/DROP it; EXAMINE it instead.\n"
                "- If the description is identical to last turn, the game state is static. INTENT: wait.\n"
                "- Never repeat the same INTENT two turns in a row.\n"
                "- Before choosing an action, check the FAILED ACTIONS list in memory. Do not repeat any action on that list.\n\n" 
                "Format:\n"
                "REASONING: <One sentence on the current goal>\n"
                "INTENT: <The logical interaction>"
            ),
            description="Prompt for the DeepSeek Strategist agent",
        )

        PROMPT_ENCODER: str = Field(
            default=(
                "Convert a natural-language INTENT into exactly one Z-machine parser command.\n\n"
                "Allowed verbs (use only these):\n"
                "  Movement:   N, S, E, W, NE, NW, SE, SW, U, D\n"
                "  Examine:    X <NOUN>  (never 'EXAMINE', always 'X')\n"
                "  Look:       LOOK  (no noun, surveys the room)\n"
                "  Inventory:  I  (no noun, lists carried items)\n"
                "  Take:       GET <NOUN>\n"
                "  Drop:       DROP <NOUN>\n"
                "  Wear:       WEAR <NOUN>\n"
                "  Open:       OPEN <NOUN>\n"
                "  Close:      CLOSE <NOUN>\n"
                "  Unlock:     UNLOCK <NOUN> WITH <NOUN>\n"
                "  Insert:     INSERT <NOUN> IN <NOUN>\n"
                "  Put:        PUT <NOUN> IN <NOUN>  or  PUT <NOUN> ON <NOUN>\n"
                "  Search:     SEARCH <NOUN>\n"
                "  Push:       PUSH <NOUN>\n"
                "  Pull:       PULL <NOUN>\n"
                "  Turn:       TURN <NOUN>\n\n"
                "Rules:\n"
                "- Never use articles (no THE, A, AN).\n"
                "- Use the shortest, most unique noun (JUMPSUIT not CLOTHES, CYLINDER not OBJECT).\n"
                "- For 'check inventory' or 'inventory': output COMMAND: I\n"
                "- For 'look around' or 'survey room': output COMMAND: LOOK\n"
                "- For 'unlock door with card': output COMMAND: UNLOCK DOOR WITH CARD\n"
                "- For 'insert cylinder in port': output COMMAND: INSERT CYLINDER IN PORT\n\n"
                "Output format — one line, nothing else:\n"
                "COMMAND: <instruction>\n\n"
                "Do not output any other text, explanation, markdown, or punctuation. "
                "If you are uncertain, default to: COMMAND: LOOK"
            ),
            description="Prompt for the Gemini Encoder agent",
        )

    def __init__(self):
        self.valves = self.Valves()

    def play_game(self, max_turns: int = 50) -> str:
        """Runs the autonomous playtest loop."""
        if not self.valves.GEMINI_API_KEY or not self.valves.DEEPSEEK_API_KEY:
            return "Error: API keys missing in Valves."
        return run_playtester(self.valves, max_turns)