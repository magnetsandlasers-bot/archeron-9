! ==============================================================================
! THE GAME MAP (Objects & Rooms Only)
! ==============================================================================

! --- 1. Cryo Bay ---
Object  CryoBay "Cryo Bay"
  with  description
           "The air is freezing here. Rows of frosted glass tubes line the walls. 
            Most are shattered. The only exit is a bulkhead door leading north.",
        n_to Corridor,
  has   light;

Object  -> PlayerCryoTube "cryo tube"
  with  name 'cryo' 'tube' 'pod' 'glass',
        description [;
            print "The tube you emerged from. The label reads: ";
            PrintVisual("SUBJECT: 009 - BIOLOGICAL ASSET");
            print "^";
            return true;
        ],
  has   static;

Object  -> DepletedCell "heavy fusion cell"
  with  name 'cell' 'battery' 'fusion' 'power' 'unit',
        description "A heavy cylinder. It feels dense.",
        bulk BULK_HEAVY,
        initial "A heavy fusion cell lies on the floor near your tube.";

Object  ChargedCell "charged fusion cell"
  with  name 'cell' 'battery' 'fusion' 'power' 'unit',
        bulk BULK_HEAVY,
        description "A heavy fusion battery. It hums with blue energy.";

! --- 2. The Corridor ---
Object  Corridor "The Spine"
  with  description [;
           print "A long, dark metal corridor. Emergency lights flicker.^";
           if (lockdown_lifted == 0) {
               print "To the north, a massive bulkhead door blocks the way.^";
           } else {
               print "To the north, the massive bulkhead door stands open.^";
           }
           print "To the east, a security door sits flush with the wall.
           To the west is the Reactor Control Room.^";
           return true;
        ],
        s_to CryoBay,
        w_to ReactorRoom,
        n_to BulkheadDoor,      
        e_to EngineeringDoor,   
  has   light;

Object  -> BulkheadDoor "bulkhead door"
  with  name 'door' 'bulkhead' 'massive' 'north' 'blast',
        description [;
            print "A massive blast door. ";
            if (lockdown_lifted == 0) {
                print "A hologram reads: "; PrintVisual("SECTOR LOCKDOWN"); print ".^";
            } else {
                print "It is currently open.^";
            }
            return true;
        ],
        door_dir n_to,
        door_to Atrium,
  has   static door locked scenery; 

Object  -> Corpse "desiccated corpse"
  with  name 'body' 'corpse' 'man' 'crewman' 'dead' 'uniform',
        description [;
            print "A crew member, long dead. Desiccated skin pulled tight against bone.";
            if (Keycard in self) {
                 give Keycard ~concealed; 
                 print " You notice a plastic card in the pocket.";
            }
            print "^";
            return true;
        ],
  has   static container open;

Object  -> -> Keycard "plastic keycard"
  with  name 'card' 'key' 'keycard' 'pass',
        bulk BULK_POCKET,
        description [;
            print "A plastic card. The label reads: ";
            PrintVisual("ENGINEERING ACCESS");
            print ".^";
            return true;
        ],
  has   concealed; 

! --- SHARED OBJECTS (Engineering Door) ---
Object  EngineeringDoor "security door"
  with  name 'door' 'security' 'heavy' 'engineering' 'east' 'west',
        description [;
            if (location == Corridor) {
                print "A heavy steel door with a card reader slot.";
                if (power_restored == 0) print " The display is dead.";
                else {
                     print " The display reads: ";
                     PrintVisual("RESTRICTED AREA");
                }
            } else {
                print "The inner face is smooth steel.";
            }
            print "^";
            return true;
        ],
        found_in Corridor Engineering,
        door_dir [; if (location == Corridor) return e_to; return w_to; ],
        door_to [; if (location == Corridor) return Engineering; return Corridor; ],
        with_key Keycard,
        before [;
            Unlock, Open:
                if (location == Engineering) "Use the green panel.";
                if (power_restored == 0) "The electronic lock is dead. It needs power.";
        ],
  has   static door openable lockable locked scenery;

! --- 3. Engineering ---
Object  Engineering "Engineering Deck"
  with  description "A massive industrial chamber. To the west is the security door.
                     A Hydraulic Control Station dominates the far wall.",
        w_to EngineeringDoor,
  has   light;

Object  -> ExitPanel "green panel"
  with  name 'panel' 'button' 'green',
        before [;
            Push:
                print "The door slides open.^";
                give EngineeringDoor ~locked open;
                return true;
        ],
  has   static scenery;

Object  -> ControlStation "control station"
  with  name 'station' 'hydraulic' 'control',
        description "It features a Red Wheel and a Heavy Iron Lever.",
  has   static scenery supporter;

Object  -> RedWheel "red wheel" 
  with  name 'wheel' 'valve' 'red',
        before [;
            Turn:
                if (power_restored == 0) "Locked tight.";
                if (self has open) "Already turned.";
                give self open; 
                print "You heave the wheel. Steam blasts from a vent!^";
                return true;
        ],
  has   static scenery; 

Object  -> Lever "iron lever"
  with  name 'lever' 'handle',
        before [;
            Pull:
                if (power_restored == 0) "Nothing happens.";
                if (lockdown_lifted) "Already pulled.";
                if (RedWheel hasnt open) "The lever is stuck. A safety pin blocks it.";
                
                lockdown_lifted = 1;
                give BulkheadDoor ~locked open;
                print "KLANNNG! The bulkhead door in the Corridor opens.^";
                return true;
        ],
  has   static scenery;

! --- 4. Reactor Control ---
Object  ReactorRoom "Reactor Control Room"
  with  description [;
           print "The nerve center. A charger unit is on the wall. The Main Reactor is here.";
           if (power_restored) print " The reactor hums with power.";
           else print " The room is silent.";
           print "^";
           return true;
        ],
        e_to Corridor,
  has   light;

Object  -> Charger "wall unit"
  with  name 'charger' 'station' 'slot',
        description "A wall-mounted charging port.",
        before [;
            Receive:
                if (child(self) ~= 0) "Full.";
                if (noun == DepletedCell) {
                    remove DepletedCell;
                    move ChargedCell to self;
                    print "The machine whirs, charging the cell.^";
                    return true;
                }
        ],
  has   static container open scenery;

Object  -> Reactor "main reactor"
  with  name 'main' 'reactor' 'core',
        before [;
            Receive:
                if (child(self) ~= 0) "Full.";
                if (noun == ChargedCell) {
                    power_restored = 1;
                    remove ChargedCell; 
                    print "You slide the cell in. POWER RESTORED! The lights flicker on.^";
                    return true;
                }
        ],
  has   static container open scenery;

! --- 5. The Central Atrium ---
Object  Atrium "Central Atrium"
  with  description
           "A massive, multi-level atrium.
            ^
            North: The Bridge (Locked).
            South: The Spine.
            East: Mess Hall (Crew Sector).
            West: Security Checkpoint.
            Down: A ladder leads to the dark Cargo Bay.",
        s_to Corridor,
        e_to MessHall,
        w_to SecurityCheck,
        n_to BridgeDoor,
        d_to CargoBay,
  has   light;

! --- 6. Comms Relay (Audio Upgrade) ---
Object  CommsRelay "Comms Relay"
  with  description
           "Banks of radio equipment line the walls. Most are burnt out.",
        w_to Atrium,
  has   light;

Object  -> AudioModule "silver component"
  with  name 'component' 'module' 'chip' 'card' 'silver',
        bulk BULK_POCKET,
        description "A sleek silver circuit board. It looks like a standard Decryption Module.",
        before [;
            Wear:
                <<Install self>>;
        ],
        react_after [;
            Install:
                if (noun == self) {
                    if (audio_codec_active == 1) "System already updated.";
                    audio_codec_active = 1;
                    remove self;
                    print "You slide the module into the port behind your ear. It clicks into place.
                    ^
                    [SYSTEM UPDATE: AUDIO DRIVERS RESTORED]
                    ^
                    The static in the air resolves into a voice.
                    ARPI: '...Subject 9? I'm ARPI. I'm in the Bridge.'^";
                    return true;
                }
        ];

! --- 7. Server Room (Visual Upgrade & Identity Puzzle) ---
Object  ServerRoom "Server Room"
  with  description 
            "Rows of server banks hum quietly. A main terminal sits in the center.",
        e_to Atrium,
  has   light;

Object  -> OpticalModule "optical driver"
  with  name 'driver' 'module' 'board' 'green',
        bulk BULK_POCKET,
        description "A green circuit board labeled 'OPTICAL DRIVER v4.0'.
                     It says: 'Enables HUD text translation and Blink-Link image transfer.'",
        before [;
            Wear:
                <<Install self>>;
        ],
        react_after [;
            Install:
                if (noun == self) {
                    if (visual_cortex_active == 1) "System already updated.";
                    visual_cortex_active = 1;
                    remove self;
                    print "You open the maintenance hatch on your forearm and slot the board in.
                    ^
                    [SYSTEM UPDATE: OPTICAL HUD RESTORED]
                    ^
                    The swimming glyphs on the terminal screen snap into focus.^";
                    return true;
                }
        ];

Object  -> ServerTerminal "main terminal"
  with  name 'terminal' 'computer' 'screen' 'code' 'image' 'qr' 'glyph',
        qr_content "PROTOCOL 99-DELTA: To override Asset status, insert a standard Admin Wafer into the slot.",
        description [;
            print "The screen displays the ship's manifest. ";
            if (visual_cortex_active == 0) {
                 print "You cannot read it yet.";
            } else {
                 print "CURRENT USER: SUBJECT 009 (ASSET). ACCESS: RESTRICTED.
                 ^
                 In the corner of the screen is a complex square data-glyph (QR).";
                 if (identity_hacked) print "^STATUS: OVERRIDDEN (CREW ACCESS).";
                 
                 ! -- HUD TRIGGER --
                 last_qr_source = self;
                 print "^^[HUD: DATA-GLYPH DETECTED. BLINK TWICE TO DOWNLOAD]";
            }
            print "^";
            return true;
        ],
  has   static scenery;

Object  -> TerminalSlot "data slot"
  with  name 'slot' 'drive' 'reader',
        description "A slot designed for a standard Admin Data Wafer.",
        before [;
            Receive:
                if (noun == DataWafer) {
                    if (visual_cortex_active == 0) {
                        print "You can't read the screen to operate the hack.^";
                        return true;
                    }
                    remove DataWafer;
                    identity_hacked = 1;
                    print "You insert the wafer. You navigate the menus and locate your file:
                    SUBJECT 009.
                    ^
                    You delete the 'ASSET' tag and replace it with 'CREW'.
                    [IDENTITY UPDATE COMPLETE]^";
                    return true;
                }
        ],
  has   static container open scenery;

! --- 8. Cargo Bay (Wafer) ---
Object  CargoBay "Cargo Bay"
  with  description
           "A dark, cavernous storage area. Crates are stacked high.",
        u_to Atrium,
  has   light; 

Object  -> CargoCrates "crates"
  with  name 'crate' 'crates',
        description "Standard shipping crates.",
  has   static scenery;

Object  -> DataWafer "data wafer"
  with  name 'wafer' 'disk' 'chip' 'admin',
        bulk BULK_POCKET,
        description [;
            print "A small data chip. Label: ";
            PrintVisual("ADMINISTRATIVE ACCESS");
            print ".^";
            return true;
        ];

! --- 9. The Bridge (ARPI Interaction) ---
Object  BridgeDoor "blast door"
  with  name 'door' 'blast' 'bridge' 'north',
        description "A reinforced door protecting the Bridge. It has a biometric scanner.",
        before [;
            Open, Unlock:
                if (location == Bridge) {
                    give self ~locked open;
                    "Unlocked.";
                }
                print "The scanner sweeps over you. ";
                if (identity_hacked == 0) {
                    print "^VOICE: ";
                    PrintAudio("ACCESS DENIED. BIOLOGICAL ASSET DETECTED.");
                    print "^The door remains locked.^";
                    return true;
                }
                print "^VOICE: ";
                PrintAudio("WELCOME, CREWMAN.");
                print "^The heavy bolts retract. The door opens.^";
                give self ~locked open;
                return true;
        ],
        door_dir n_to,
        door_to Bridge,
  has   static door locked scenery;

Object  Bridge "The Bridge"
  with  description "The command deck. The viewport shows the red planet Archeron-9 looming close.
                     To the North is the Escape Pod Airlock.
                     ^
                     The Mainframe Server Bank covers the back wall.",
        s_to Atrium,
        n_to Airlock,
  has   light;

Object  -> Mainframe "mainframe"
  with  name 'mainframe' 'server' 'bank' 'computer',
        description "A massive computer bank. The screen flashes: [QUARANTINE LOCK ACTIVE].
                     A heavy 'AI Core' drive is slotted into the center.",
  has   static scenery;

Object  -> ARPICore "AI Core"
  with  name 'core' 'drive' 'unit' 'box' 'arpi' 'ai',
        bulk BULK_HEAVY,
        description "A heavy, rectangular drive containing ARPI's consciousness.",
        before [;
            Take:
                if (audio_codec_active == 0) {
                    print "You don't understand what this is yet.^";
                    return true;
                }
                print "You grip the heavy core. 
                ^
                ARPI: 'Do it, 9! Pull me out! It's the only way to break the quarantine!'
                ^
                You yank the drive free. Sparks fly!
                ^
                ALARM: [CRITICAL SYSTEM FAILURE. UNAUTHORIZED REMOVAL. SELF-DESTRUCT INITIATED.]
                ^
                ARPI (from the box in your hands): 'Run! Get us to the Airlock!'^";
                arpi_downloaded = 1;
                return false; 
        ];

Object  Airlock "Escape Pod Airlock"
  with  description "The final airlock. The Escape Pod is prepped for launch.",
        s_to Bridge,
        before [;
            Go:
               if (noun == n_to) {
                   print "You stumble into the pod. ";
                   if (ARPICore in player) {
                       print "You slot ARPI's core into the pod's computer.
                       ^
                       ARPI: 'Launch sequence initiated. Hold on!'
                       ^
                       The pod blasts away just as the reactor goes critical. 
                       ^
                       You drift towards the red planet.
                       ^
                       ARPI: 'We aren't landing on the surface, 9. Look.'
                       ^
                       You look out the window. A ring of silver satellites glitters in the darkness 
                       above the dead world.
                       ^
                       ARPI: 'That is where the answers are.'
                       ^
                       *** TO BE CONTINUED IN ARCHERON-10 ***^";
                       deadflag = 2;
                       return true;
                   } else {
                       print "You sit in the pilot seat. But the computer is dead. 
                       It needs an AI pilot to calculate the trajectory. 
                       ^
                       The ship explodes around you.
                       ^
                       *** GAME OVER ***^";
                       deadflag = 1;
                       return true;
                   }
               }
        ],
        n_to "Escape Pod",
  has   light;

! --- EXTRA ROOMS FOR DRONE LOGIC ---

Object  SecurityCheck "Security Checkpoint"
  with  description "A reinforced checkpoint. Blast shutters cover the windows. 
                     Automated turrets hang from the ceiling.
                     To the west is the Ops Center.
                     To the east is the Atrium.",
        e_to Atrium,
        w_to OpsCenter,
  has   light;

Object  OpsCenter "Ops Center"
  with  description "Banks of monitors display the ship's status. 
                     A Recall Terminal sits here.",
        e_to SecurityCheck,
  has   light;

Object  -> RecallTerminal "recall terminal"
  with  name 'terminal' 'screen' 'console',
        description [;
            print "The screen controls the automated security systems. ";
            if (identity_hacked) {
                print "It is active. You see a command: [RECALL PATROL DRONE].";
            } else {
                print "It is locked. [ACCESS DENIED: CREW ONLY].";
            }
            print "^";
            return true;
        ],
        before [;
            Recall, Push:
                if (identity_hacked == 0) "Access Denied.";
                if (drone_active == 0) "The drone is already recalled.";
                drone_active = 0;
                move SecurityDrone to nothing;
                print "You type the command.
                ^
                [COMMAND ACCEPTED]
                ^
                The drone emits a powering-down whistle and flies into a wall chute. 
                The halls are safe now.^";
                return true;
        ],
  has   static scenery;

! --- THE DRONE ---
Object  SecurityDrone "Security Drone"
  with  name 'drone' 'robot' 'machine' 'patrol',
        description "A floating sphere of black metal with a glowing red eye and a taser mount.",
        initial "A Security Drone hovers here, scanning for movement.",
  has   static; 

! --- EXTRA ROOMS FOR DRONE PATHING ---
Object  MessHall "Mess Hall"
  with  description "Overturned tables. To the East is Crew Quarters.",
        w_to Atrium,
        e_to CrewQuarters,
  has   light;

Object  CrewQuarters "Crew Quarters"
  with  description "Rows of bunks. To the South is Medical Bay.",
        w_to MessHall,
        s_to MedicalBay,
  has   light;

Object  MedicalBay "Medical Bay"
  with  description "A sterile room.",
        n_to CrewQuarters,
  has   light;

Object  ServerFarm "Server Farm"
  with  description "Towering black server racks.",
        s_to OpsCenter,
  has   light;