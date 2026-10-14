#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"

/*
 * Fire-rate chip pickup. Shares the electricity pickup's sheet (frames 1-2:
 * a microchip with a ">>" speed mark that shimmers between two shades).
 * Touching it sets pending_firechip_pickup; StateGame grants rapid fire on
 * its next safe tick (same deferred pattern as the electricity pickup).
 */

#define PICKUP_RANGE 12
#define SHIMMER_FRAMES 16

extern Sprite* scroll_target;
extern UINT8 pending_firechip_pickup;

void START() {
    SetFrame(THIS, 1);
    THIS->custom_data[0] = SHIMMER_FRAMES;
    THIS->coll_w = 0;
    THIS->coll_h = 0;
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    INT16 dx;
    INT16 dy;

    if (--THIS->custom_data[0] == 0) {
        THIS->custom_data[0] = SHIMMER_FRAMES;
        SetFrame(THIS, THIS->anim_frame == 1 ? 2 : 1);
    }
    if (!scroll_target) {
        return;
    }
    dx = (INT16)scroll_target->x - (INT16)THIS->x;
    dy = (INT16)scroll_target->y - (INT16)THIS->y;
    if (dx < -PICKUP_RANGE || dx > PICKUP_RANGE || dy < -PICKUP_RANGE || dy > PICKUP_RANGE) {
        return;
    }
    pending_firechip_pickup = 1;
    SpriteManagerRemove(THIS_IDX);
}

void DESTROY() {
}
