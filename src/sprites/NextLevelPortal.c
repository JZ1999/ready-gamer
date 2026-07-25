#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "StateGame.h"

#define CD_ANIM_TIMER      0
#define PORTAL_ANIM_SPEED  6
#define PORTAL_FRAME_FIRST 4
#define PORTAL_FRAME_LAST  8

extern Sprite* scroll_target;

void START() {
    SetFrame(THIS, PORTAL_FRAME_FIRST);
    THIS->custom_data[CD_ANIM_TIMER] = PORTAL_ANIM_SPEED;
    /* Wide maps (map4/map5) place the portal far from spawn; default lim 32
       culls it before the camera ever reaches it. */
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    if (--THIS->custom_data[CD_ANIM_TIMER] == 0) {
        THIS->custom_data[CD_ANIM_TIMER] = PORTAL_ANIM_SPEED;

        if (THIS->anim_frame >= PORTAL_FRAME_LAST) {
            SetFrame(THIS, PORTAL_FRAME_FIRST);
        } else {
            SetFrame(THIS, THIS->anim_frame + 1);
        }
    }

    if (scroll_target && CheckCollision(THIS, scroll_target)) {
        pending_room_transition = 1;
    }
}

void DESTROY() {
}
