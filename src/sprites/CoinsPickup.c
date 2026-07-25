#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "SpriteData.h"
#include "SoundEffects.h"

#define PICKUP_RANGE 12
#define COIN_PICKUP_VALUE 5

extern Sprite* scroll_target;
extern UINT16 ready_coins;

void START() {
    SetFrame(THIS, 0);
    THIS->coll_w = 0;
    THIS->coll_h = 0;
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    INT16 dx;
    INT16 dy;

    if (!scroll_target) {
        return;
    }

    dx = (INT16)scroll_target->x - (INT16)THIS->x;
    dy = (INT16)scroll_target->y - (INT16)THIS->y;

    if (dx < -PICKUP_RANGE || dx > PICKUP_RANGE || dy < -PICKUP_RANGE || dy > PICKUP_RANGE) {
        return;
    }

    ready_coins += COIN_PICKUP_VALUE;
    PlayCoinCollectSound();
    SpriteManagerRemove(THIS_IDX);
}

void DESTROY() {
}
