#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "SpriteData.h"

#define CD_BOMB_FUSE       0   // Countdown frames until explosion
#define CD_EXPLODE_TIMER   1   // Frames until next explosion frame (0 = still fusing)

#define BOMB_FUSE               240  // ~4 seconds at 60fps
#define BOMB_FRAME_IDLE         0    // GBTD frame 1 (bomb) — export tiles 1-4 as frames 0-3
#define BOMB_FRAME_EXPLODE_LAST 3    // GBTD frames 2-4 (explosion burst then empty)
#define EXPLODE_FRAME_DELAY     10    // ~5 frames per step for a quick burst

extern Sprite* scroll_target;

void TakeDamage(Sprite* player);

void START() {
    SetFrame(THIS, BOMB_FRAME_IDLE);
    THIS->custom_data[CD_BOMB_FUSE] = BOMB_FUSE;
    THIS->custom_data[CD_EXPLODE_TIMER] = 0;
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    if (THIS->custom_data[CD_EXPLODE_TIMER] == 0) {
        if (THIS->custom_data[CD_BOMB_FUSE] > 0) {
            THIS->custom_data[CD_BOMB_FUSE]--;
            return;
        }

        if (scroll_target && CheckCollision(THIS, scroll_target)) {
            TakeDamage(scroll_target);
        }

        SetFrame(THIS, BOMB_FRAME_IDLE + 1);
        THIS->custom_data[CD_EXPLODE_TIMER] = EXPLODE_FRAME_DELAY;
        return;
    }

    if (--THIS->custom_data[CD_EXPLODE_TIMER] == 0) {
        if (THIS->anim_frame >= BOMB_FRAME_EXPLODE_LAST) {
            SpriteManagerRemove(THIS_IDX);
            return;
        }

        SetFrame(THIS, THIS->anim_frame + 1);
        THIS->custom_data[CD_EXPLODE_TIMER] = EXPLODE_FRAME_DELAY;
    }
}

void DESTROY() {
}
