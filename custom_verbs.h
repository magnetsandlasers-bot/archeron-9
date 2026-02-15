! ==============================================================================
! CUSTOM VERBS & INITIALISATION
! ==============================================================================

! --- Custom Action Subroutines ---

[ InstallSub;
    if (noun == nothing) "Install what?";
    "That doesn't seem to be something you can install.";
];

[ BlinkSub;
    "You blink.";
];

[ BlinkTwiceSub;
    if (visual_cortex_active == 0) {
        print "You blink, but you lack the optical drivers to process any data.^";
        return true;
    }
    if (last_qr_source == 0 || (last_qr_source notin location && last_qr_source ~= location)) {
        print "No active data stream detected in your field of view.^";
        return true;
    }
    if (last_qr_source provides qr_content) {
        print "[DOWNLOADING...]^";
        print (string) last_qr_source.qr_content;
        print "^[DOWNLOAD COMPLETE]^";
        return true;
    }
    print "Error: Data corrupted.^";
    return true;
];

[ FreezeSub;
    player_hidden = 1;
    print "You lock your servos and stand perfectly still, mimicking a statue. 
    Your thermal output drops to zero.^";
    return true;
];

[ HideSub;
    player_hidden = 1;
    print "You dive behind the nearest cover and curl into a ball. 
    You hold your breath (even though you don't need to).^";
    return true;
];

[ RecallSub;
    "You can't recall that.";
];

! --- Debug Subroutines ---
Ifdef DEBUG;
[ WarpSub;
    power_restored = 1;
    oxygen = 100;
    lockdown_lifted = 1;
    move ChargedCell to Reactor; 
    give BulkheadDoor ~locked open;
    give EngineeringDoor ~locked open;
    PlayerTo(Atrium);
    print "[DEBUG] Warped to Mid-Game.^";
    return true;
];
[ CheatSub;
    visual_cortex_active = 1;
    audio_codec_active = 1;
    identity_hacked = 1;
    print "[DEBUG] All Upgrades Installed.^";
    return true;
];
[ QuietSub;
    drone_active = 0;
    move SecurityDrone to nothing;
    print "[DEBUG] Drone Disabled.^";
    return true;
];
Endif;

! --- Verb Definitions ---

Verb 'install'
    * noun -> Install;

Verb 'blink'
    * -> Blink
    * 'twice' -> BlinkTwice;

Verb 'freeze' 'stop'
    * -> Freeze;

Verb 'hide' 'duck'
    * -> Hide;

Verb 'recall'
    * noun -> Recall;

Ifdef DEBUG;
Verb 'warp' * -> Warp;
Verb 'cheat' * -> Cheat;
Verb 'quiet' * -> Quiet;
Endif;

! --- Game Initialisation ---

[ Initialise;
    location = CryoBay;
    player.description = "You look down at your body. Grey jumpsuit. 'SUBJECT 009'. 
    Under the torn sleeve, your skin is synthetic, pale, and perfect.
    You spot several empty interface ports on your arms and neck.";
    
    print "^^System Reboot...^Memory Check... Incomplete.^You wake up gasping.^
    ^
    (ALERT: LIFE SUPPORT OFFLINE. OXYGEN LEVELS CRITICAL.)^";
];