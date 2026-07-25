#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "Scroll.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "Music.h"
#include "main.h"

#include <gb/gb.h>

// Import the simple menu tiles and tilemap for testing
IMPORT_TILES(MenuTileset);
IMPORT_MAP(MenuTilemap);

// Import menu music
DECLARE_MUSIC(menu);
DECLARE_MUSIC(readygames);

extern INT8 scroll_h_border;

/**
 * Initializes the main menu state
 * Sets up the background tilemap and prepares the menu for display
 */
void START() {
    /* Clear HUD window left over from StateGame / broken Game Over. */
    HIDE_WIN;
    SetWindowY(144);
    scroll_h_border = 0;
    scroll_offset_x = 0;
    scroll_offset_y = 0;
    scroll_target = 0;

    SpriteManagerReset();

    // Initialize scroll with the optimized menu tiles and tilemap from GB Studio
    // This uses the properly optimized 157 tiles (under the 192 limit)
    InitScrollWithTiles(BANK(MenuTilemap), &MenuTilemap, BANK(MenuTileset), &MenuTileset, 0, 0);
    
    // Show background and sprites
    SHOW_BKG;
    HIDE_SPRITES;
    
    // Play menu music
    PlayMusic(menu, LOOP);
}

/**
 * Updates the main menu state
 * Handles input for menu navigation and state transitions
 */
void UPDATE() {
    // Check for START button press to begin the game
    if (KEY_TICKED(J_START) || KEY_TICKED(J_A)) {
        PlayMusic(readygames, 0);
        SetState(StateGame);
    }
}
