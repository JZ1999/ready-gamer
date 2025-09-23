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
}

void UPDATE() {
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
