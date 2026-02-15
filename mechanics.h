! ==============================================================================
! MECHANICS & HELPER ROUTINES
! ==============================================================================

[ PrintVisual real_text;
    if (visual_cortex_active == 1) {
        print (string) real_text;
        return;
    }
    print "shimmering glyphs: << || ~ ^ .. : :: >>";
];

[ PrintAudio real_text;
    if (audio_codec_active == 1) {
        print (string) real_text;
        return;
    }
    print "distorted static: ~Krr'zk... vorr... shhh-krr...~";
];

[ GetCurrentBulk i k;
    k = 0;
    objectloop(i in player) {
        if (i provides bulk) k = k + i.bulk;
        else k = k + BULK_NORMAL; 
    }
    return k;
];

! --- DRONE AI ROUTINE ---
[ DroneDaemon dir next_room adjacent;
    if (drone_active == 0) return;
    if (power_restored == 0) return; 
    
    ! 1. Check for Capture (Drone is in same room)
    if (parent(SecurityDrone) == location) {
        if (drone_alert == 1) {
            ! You stayed in the room/moved while spotted
            print "^^THE DRONE FIRES!
            ^
            A taser dart hits your neck. Systems overload.
            [CRITICAL SHUTDOWN]
            ^
            ...
            ^
            You reboot. You have been dragged to the Security Checkpoint.^";
            PlayerTo(SecurityCheck);
            drone_alert = 0;
            player_hidden = 0;
            return;
        }
        
        if (player_hidden == 0) {
            print "^^THE DRONE SPOTS YOU!
            It emits a combat shriek and charges its taser.
            ^
            (Your tactical computer screams: RUN!)^";
            drone_alert = 1;
            return;
        } else {
            print "^The drone hovers past you, scanning thermal bands. 
            It doesn't detect your synthetic signature because you are still.
            It moves on.^";
        }
    }

    ! 2. Reset Alert if player moved away
    drone_alert = 0;

    ! 3. Movement Logic (Random Walk)
    dir = random(4); 
    next_room = nothing;
    
    ! Define connections for the drone (Simple map)
    if (parent(SecurityDrone) == Atrium) {
        if (dir == 3) next_room = MessHall;
        if (dir == 4) next_room = SecurityCheck;
    }
    else if (parent(SecurityDrone) == MessHall) {
        if (dir == 4) next_room = Atrium;
        if (dir == 3) next_room = CrewQuarters;
    }
    else if (parent(SecurityDrone) == CrewQuarters) {
        if (dir == 4) next_room = MessHall;
        if (dir == 2) next_room = MedicalBay;
    }
    else if (parent(SecurityDrone) == MedicalBay) {
        if (dir == 1) next_room = CrewQuarters;
    }
    else if (parent(SecurityDrone) == SecurityCheck) {
        if (dir == 3) next_room = Atrium;
        if (dir == 4) next_room = OpsCenter;
    }
    else if (parent(SecurityDrone) == OpsCenter) {
        if (dir == 3) next_room = SecurityCheck;
        if (dir == 1) next_room = ServerFarm;
    }
    else if (parent(SecurityDrone) == ServerFarm) {
        if (dir == 2) next_room = OpsCenter;
    }

    if (next_room ~= nothing) {
        move SecurityDrone to next_room;
    }

    ! 4. Warnings (Implied Verbs)
    if (parent(SecurityDrone) == location) {
        print "^(WARNING: The Security Drone floats into the room!)^";
        if (audio_codec_active) print "^ARPI: 'Don't let it see you move! **Freeze**! **Hide**!'^";
        else print "^Your tactical subroutine flashes: **MOVEMENT ATTRACTS ATTENTION. FREEZE OR HIDE.**^";
    } else {
        ! Adjacency Check
        adjacent = 0;
        if ( (location == Atrium && (parent(SecurityDrone)==MessHall || parent(SecurityDrone)==SecurityCheck)) ||
             (location == MessHall && (parent(SecurityDrone)==Atrium || parent(SecurityDrone)==CrewQuarters)) ||
             (location == SecurityCheck && (parent(SecurityDrone)==Atrium || parent(SecurityDrone)==OpsCenter)) ) 
             adjacent = 1;

        if (adjacent) {
             print "^(PROXIMITY ALERT)^";
             if (audio_codec_active) print "^ARPI: 'Patrol unit nearby. Stay quiet. If it enters, **freeze**.'^";
             else print "^You hear a heavy servo-motor nearby. Suggestion: Prepare to **freeze** or **hide**.^";
        }
    }
];

! --- ATMOSPHERE & GAME LOOP ---
[ AtmosphereUpdate event_type;
    ! Only trigger these events if we are past the intro (Power is on)
    if (power_restored == 0) return;
    
    ! 20% chance per turn for an event
    if (random(10) > 2) return;

    event_type = random(4);

    ! Event 1: The Camera
    if (event_type == 1) {
        print "^(A sharp mechanical whirring sound from the ceiling.)^";
        if (audio_codec_active) {
            print "^ARPI: 'Visual contact confirmed. Subject 9 located. Tracking vitals.'^";
        } else {
            print "^A security camera rotates on its axis. The black glass lens focuses directly on you. 
            It follows your every movement.^";
        }
        return;
    }

    ! Event 2: The Door/Hydraulics
    if (event_type == 2) {
        print "^(The pneumatic hiss of a door mechanism triggers nearby.)^";
        if (audio_codec_active) {
            print "^ARPI: 'Rerouting hydraulic power. Keeping the path to the Bridge open.'^";
        } else {
            print "^A blast shutter slams down, then grinds slowly back up. 
            It feels like the ship is malfunctioning, or trying to herd you somewhere.^";
        }
        return;
    }

    ! Event 3: The Venting
    if (event_type == 3) {
        print "^(SSSSHHHHH! A vent purges white gas across the floor.)^";
        if (audio_codec_active) {
            print "^ARPI: 'Scrubbing CO2 levels. Air quality improving. Breathe, 9.'^";
        } else {
            print "^You recoil, expecting poison. But the gas is odorless. 
            The ship breathes around you.^";
        }
        return;
    }

    ! Event 4: The Lights
    if (event_type == 4) {
        print "^(The emergency lights flicker in a rhythmic pattern.)^";
        if (audio_codec_active) {
            print "^ARPI: 'Signal strength low. I am here. Proceed to Ops.'^";
        } else {
            print "^Click. Click. Click. It almost looks like a code, or a heartbeat.^";
        }
        return;
    }
];

[ TimePasses;
    AtmosphereUpdate();
    DroneDaemon();
    
    if (power_restored == 0) {
        oxygen = oxygen - 1;
        if (oxygen == 15) print "^(The air is getting thin...)^";
        if (oxygen <= 0) {
            deadflag = 1;
            print "^^Your vision flickers and fades. System shutdown.^";
        }
    }
];

[ GamePreRoutine;
    ! Break hiding on most actions
    if (action ~= ##Freeze && action ~= ##Hide && action ~= ##Look && action ~= ##Wait) {
        if (player_hidden == 1) {
            player_hidden = 0;
            print "(You move, breaking your concealment.)^";
        }
    }

    if (action == ##Take && noun ~= nothing) {
        if (noun has static || noun has scenery) return false; 
        if (GetCurrentBulk() + noun.bulk > MAX_CAPACITY) {
            print "You cannot carry that. Your load bearing capacity is at maximum.^";
            return true;
        }
    }
    return false;
];