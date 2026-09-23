#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"

/*
 * Invisible sprite whose only job is to be `scroll_target` during
 * StateBossRun: the engine auto-follows scroll_target->x every frame
 * (SpriteManager.c calls RefreshScroll() right after this sprite updates),
 * so incrementing this sprite's own x each frame gets auto-scroll for
 * free, reusing the exact same camera code the room system already uses
 * for player-follow — no new scroll-driving code needed.
 */

#define CD_FRAME_DIVIDER 0

#define AUTOSCROLL_FRAME_DIVIDER 2 /* advance 1px every N frames — ~30px/s at 60fps */

void START() {
    THIS->coll_w = 0;
    THIS->coll_h = 0;
    THIS->y = 200; /* below the map's 144px height — this sprite is never meant to be seen */
    THIS->lim_x = 255;
    THIS->lim_y = 255;
    THIS->custom_data[CD_FRAME_DIVIDER] = AUTOSCROLL_FRAME_DIVIDER;
}

void UPDATE() {
    if (--THIS->custom_data[CD_FRAME_DIVIDER] == 0) {
        THIS->custom_data[CD_FRAME_DIVIDER] = AUTOSCROLL_FRAME_DIVIDER;
        THIS->x++;
    }
}

void DESTROY() {
}
