#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "SpriteData.h"

#define CD_FRAME_TIMER 1

#define ENEMY_SPEED 5
#define TOTAL_FRAMES 3

extern Sprite* scroll_target;

void START() {
    THIS->custom_data[CD_FRAME_TIMER] = ENEMY_SPEED;
    THIS->custom_data[CD_ENEMY_HEALTH] = 3;
    THIS->custom_data[CD_MOVE_TIMER] = 0;
}

void UPDATE() {
    UINT8* move_timer = &THIS->custom_data[CD_MOVE_TIMER];

    if ((*move_timer)++ < ENEMY_SPEED) return;
    *move_timer = 0;

    UINT16 dx = 0;
    UINT16 dy = 0;

    // Determine movement direction toward player
    if(scroll_target->x > THIS->x + 1) dx = 1;
    else if(scroll_target->x < THIS->x - 1) dx = -1;
    else if(scroll_target->y > THIS->y + 1) dy = 1;
    else if(scroll_target->y < THIS->y - 1) dy = -1;

    // Tile collision check
    UINT16 new_x = THIS->x + dx;
    UINT16 new_y = THIS->y + dy;

    // Blink logic
    if (THIS->custom_data[CD_BLINK_TIMER] > 0) {
        THIS->custom_data[CD_BLINK_TIMER]--;

        if ((THIS->custom_data[CD_BLINK_TIMER] / 2) % 2 == 0) {
            SetFrame(THIS, 3); // 3 = blank frame (must exist in your sprite)
        }
    } else {
        if((--THIS->custom_data[CD_FRAME_TIMER]/10) == 0) {
            THIS->custom_data[CD_FRAME_TIMER] = ENEMY_SPEED;
            SetFrame(THIS, (THIS->anim_frame + 1) % TOTAL_FRAMES);
        }
    }

    TranslateSprite(THIS, dx, dy);
}

void DESTROY() {
    // Cleanup if needed
}
