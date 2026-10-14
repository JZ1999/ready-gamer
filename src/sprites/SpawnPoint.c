#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"

#define ANIMATION_SPEED 8  // Frames between animation updates
#define TOTAL_FRAMES 4     // Frames 0 to 3

void START() {
    // Set initial frame
    SetFrame(THIS, 0);
    
    // Set animation timer
    THIS->custom_data[0] = ANIMATION_SPEED;
    
    // Spawn points should not have collision
    THIS->coll_w = 0;
    THIS->coll_h = 0;

    /* Persist on wide maps where spawners start off-camera. */
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    /* Warning: an enemy appears here in custom_data[1] frames (StateGame.c).
     * Spin 4x faster than normal and flash white, so the player can see
     * which spawner is about to open. */
    if (THIS->custom_data[1]) {
        UINT8 w = --THIS->custom_data[1];
        if ((w & 1) == 0) {
            SetFrame(THIS, (THIS->anim_frame + 1) % TOTAL_FRAMES);
        }
        SPRITE_SET_DMG_PALETTE(THIS, w ? ((w >> 2) & 1) : 0);
        return;
    }

    // Decrement animation timer
    if (--THIS->custom_data[0] == 0) {
        THIS->custom_data[0] = ANIMATION_SPEED;
        
        // Cycle through frames 0 to 3
        UINT8 current_frame = THIS->anim_frame;
        UINT8 next_frame = (current_frame + 1) % TOTAL_FRAMES;
        SetFrame(THIS, next_frame);
    }
}

void DESTROY() {
    // Cleanup if needed
}
