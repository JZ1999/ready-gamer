#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "SpriteData.h"

#define CD_FRAME_TIMER 1

#define ENEMY_SPEED 5
#define TOTAL_FRAMES 3
#define BOMB_DROP_INTERVAL 300  // 5 seconds at 60fps (needs 16-bit timer; UINT8 max is 255)

extern Sprite* scroll_target;

static void SetBombDropTimer(UINT16 frames) {
    THIS->custom_data[CD_BOMB_TIMER] = (UINT8)(frames & 0xFF);
    THIS->custom_data[CD_BOMB_TIMER_HI] = (UINT8)(frames >> 8);
}

static UINT16 GetBombDropTimer(void) {
    return THIS->custom_data[CD_BOMB_TIMER] | ((UINT16)THIS->custom_data[CD_BOMB_TIMER_HI] << 8);
}

// Custom movement function that checks for partial brick collisions
static UINT8 CustomTranslateSprite(Sprite* sprite, INT8 x, INT8 y) {
    extern UINT8 EnemyMoveWithWallAvoidance(Sprite* enemy, INT16 dx, INT16 dy);
    return EnemyMoveWithWallAvoidance(sprite, x, y);
}

void START() {
    THIS->custom_data[CD_FRAME_TIMER] = ENEMY_SPEED;
    THIS->custom_data[CD_ENEMY_HEALTH] = 3;
    THIS->custom_data[CD_MOVE_TIMER] = 0;
    SetBombDropTimer(BOMB_DROP_INTERVAL);
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    // Bomb drop: tick every frame (must run before movement early-return)
    UINT16 bomb_timer = GetBombDropTimer();
    if (bomb_timer > 0) {
        SetBombDropTimer(bomb_timer - 1);
    } else {
        SpriteManagerAdd(Bomb, THIS->x, THIS->y);
        SetBombDropTimer(BOMB_DROP_INTERVAL);
    }

    UINT8* move_timer = &THIS->custom_data[CD_MOVE_TIMER];

    if ((*move_timer)++ < ENEMY_SPEED) return;
    *move_timer = 0;

    UINT16 dx = 0;
    UINT16 dy = 0;

    // Determine movement direction toward player
    if (scroll_target->x > THIS->x + 1) dx = 1;
    else if (scroll_target->x < THIS->x - 1) dx = -1;
    else if (scroll_target->y > THIS->y + 1) dy = 1;
    else if (scroll_target->y < THIS->y - 1) dy = -1;

    UINT8 frame = THIS->anim_frame;

    // Walking animation logic
    if ((--THIS->custom_data[CD_FRAME_TIMER] / 10) == 0) {
        THIS->custom_data[CD_FRAME_TIMER] = ENEMY_SPEED;
        frame = (THIS->anim_frame + 1) % TOTAL_FRAMES;
    }

    // Hit feedback logic
    if (THIS->custom_data[CD_BLINK_TIMER] > 0) {
        THIS->custom_data[CD_BLINK_TIMER]--;
        if ((THIS->custom_data[CD_BLINK_TIMER] / 2) % 2 == 0) {
            frame = 3;
        }
    }

    SetFrame(THIS, frame);
    CustomTranslateSprite(THIS, dx, dy);
}

void DESTROY() {
}
