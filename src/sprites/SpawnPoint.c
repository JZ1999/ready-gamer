#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"

#define ANIMATION_SPEED 8  // Frames between animation updates
#define WARNING_ANIMATION_SPEED 2 // Faster spin while an enemy is about to come out
#define TOTAL_FRAMES 4     // Frames 0 to 3

void START() {
    // Set initial frame
    SetFrame(THIS, 0);

    // Set animation timer
    THIS->custom_data[CD_SPAWN_ANIM_TIMER] = ANIMATION_SPEED;
    THIS->custom_data[CD_SPAWN_WARNING] = 0;

    // Spawn points should not have collision
    THIS->coll_w = 0;
    THIS->coll_h = 0;

    /* Persist on wide maps where spawners start off-camera. */
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

// "Enemy incoming" warning, started by StateGame's SpawnEnemies: flash white
// every 4 frames using DMG palette 1 (set to solid white by SpritePlayer's
// START). Returns the animation speed to use this frame.
static UINT8 UpdateWarning(void) {
    if (THIS->custom_data[CD_SPAWN_WARNING] == 0) {
        return ANIMATION_SPEED;
    }

    THIS->custom_data[CD_SPAWN_WARNING]--;

    if (THIS->custom_data[CD_SPAWN_WARNING] != 0 &&
        (THIS->custom_data[CD_SPAWN_WARNING] & 4)) {
        SPRITE_SET_DMG_PALETTE(THIS, 1);
    } else {
        // Always end on the normal palette, so the portal never stays white.
        SPRITE_SET_DMG_PALETTE(THIS, 0);
    }
    return WARNING_ANIMATION_SPEED;
}

void UPDATE() {
    UINT8 speed = UpdateWarning();

    // Clamp so switching to the fast speed takes effect right away.
    if (THIS->custom_data[CD_SPAWN_ANIM_TIMER] > speed) {
        THIS->custom_data[CD_SPAWN_ANIM_TIMER] = speed;
    }

    // Decrement animation timer
    if (--THIS->custom_data[CD_SPAWN_ANIM_TIMER] == 0) {
        THIS->custom_data[CD_SPAWN_ANIM_TIMER] = speed;

        // Cycle through frames 0 to 3
        UINT8 current_frame = THIS->anim_frame;
        UINT8 next_frame = (current_frame + 1) % TOTAL_FRAMES;
        SetFrame(THIS, next_frame);
    }
}

void DESTROY() {
    // Cleanup if needed
}
