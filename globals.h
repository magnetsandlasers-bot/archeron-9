! ==============================================================================
! GLOBALS, CONSTANTS, & PROPERTIES
! ==============================================================================

Constant Story "Archeron-9";
Constant Headline "^An Interactive Sci-Fi Mystery^";
Constant DEBUG; 

! --- Inventory Constants ---
Constant BULK_POCKET = 1;
Constant BULK_NORMAL = 2;
Constant BULK_HEAVY  = 10;
Constant MAX_CAPACITY = 10;

! --- Global Variables ---
Global power_restored = 0;
Global lockdown_lifted = 0;
Global visual_cortex_active = 0; 
Global audio_codec_active = 0;   
Global identity_hacked = 0;      
Global oxygen = 50; 
Global arpi_downloaded = 0;
Global last_qr_source = 0;
Global drone_active = 1;         ! 1 = Roaming, 0 = Disabled
Global player_hidden = 0;        ! 1 = Frozen/Hidden
Global drone_alert = 0;          ! 1 = Drone has spotted you

! --- Custom Properties ---
Property bulk; 
Property qr_content; 