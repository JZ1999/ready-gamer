#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "Scroll.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "Music.h"

// Import the simple menu tiles and tilemap for testing
IMPORT_TILES(menu_tileset);
IMPORT_MAP(menu_tilemap);

// Import menu music
DECLARE_MUSIC(menu);

/**
 * Initializes the main menu state
 * Sets up the background tilemap and prepares the menu for display
 */
void START() {
    // Initialize scroll with the optimized menu tiles and tilemap from GB Studio
    // This uses the properly optimized 157 tiles (under the 192 limit)
    InitScrollWithTiles(BANK(menu_tilemap), &menu_tilemap, BANK(menu_tileset), &menu_tileset, 0, 0);
    
    // Show background and sprites
    SHOW_BKG;
    SHOW_SPRITES;
    
    // Clear any existing sprites
    SpriteManagerReset();
    
    // Play menu music
    PlayMusic(menu, LOOP);
}

/**
 * Updates the main menu state
 * Handles input for menu navigation and state transitions
 */
void UPDATE() {
    // Check for START button press to begin the game
    if (KEY_TICKED(J_START)) {
        SetState(StateGame);
    }
    
    // Optional: Add other menu interactions here
    // For example, A button to start game as well
    if (KEY_TICKED(J_A)) {
        SetState(StateGame);
    }
}
