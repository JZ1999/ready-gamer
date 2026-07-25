#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "SpriteData.h"
#include "StateGame.h"

/*
 * ChargeVirus — winds up, then dashes in a straight line toward the player.
 * Distinct from SpeedVirus (always fast) and BomberVirus (projectile pressure).
 * Readable on DMG: long blink telegraph, then a burst of movement.
 */

#define CD_FRAME_TIMER   1
#define CD_CHARGE_STATE  4
#define CD_CHARGE_TIMER  5
#define CD_DASH_DX       6
#define CD_DASH_DY       7

#define TOTAL_FRAMES     3
#define WALK_SPEED       12
#define WINDUP_FRAMES    24
#define DASH_STEP_DELAY  1
#define DASH_STEPS       20
#define CHARGE_COOLDOWN  100

#define STATE_CHASE   0
#define STATE_WINDUP  1
#define STATE_DASH    2

extern Sprite* scroll_target;

static UINT8 CustomTranslateSprite(Sprite* sprite, INT8 x, INT8 y) {
    return EnemyMoveWithWallAvoidance(sprite, x, y);
}

void START() {
    THIS->custom_data[CD_FRAME_TIMER] = WALK_SPEED;
    THIS->custom_data[CD_ENEMY_HEALTH] = 3;
    THIS->custom_data[CD_MOVE_TIMER] = 0;
    THIS->custom_data[CD_CHARGE_STATE] = STATE_CHASE;
    THIS->custom_data[CD_CHARGE_TIMER] = CHARGE_COOLDOWN;
    THIS->custom_data[CD_DASH_DX] = 0;
    THIS->custom_data[CD_DASH_DY] = 0;
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    UINT8 state = THIS->custom_data[CD_CHARGE_STATE];
    UINT8 frame = THIS->anim_frame;
    INT8 dx = 0;
    INT8 dy = 0;

    if (THIS->custom_data[CD_BLINK_TIMER] > 0) {
        THIS->custom_data[CD_BLINK_TIMER]--;
    }

    if (state == STATE_CHASE) {
        if (THIS->custom_data[CD_CHARGE_TIMER] > 0) {
            THIS->custom_data[CD_CHARGE_TIMER]--;
        } else {
            THIS->custom_data[CD_CHARGE_STATE] = STATE_WINDUP;
            THIS->custom_data[CD_CHARGE_TIMER] = WINDUP_FRAMES;
            THIS->custom_data[CD_BLINK_TIMER] = WINDUP_FRAMES;
        }

        if (++THIS->custom_data[CD_MOVE_TIMER] < WALK_SPEED) {
            if (THIS->custom_data[CD_BLINK_TIMER] > 0 &&
                ((THIS->custom_data[CD_BLINK_TIMER] / 2) % 2) == 0) {
                frame = 3;
            }
            SetFrame(THIS, frame);
            return;
        }
        THIS->custom_data[CD_MOVE_TIMER] = 0;

        if (scroll_target) {
            if (scroll_target->x > THIS->x + 1) dx = 1;
            else if (scroll_target->x < THIS->x - 1) dx = -1;
            else if (scroll_target->y > THIS->y + 1) dy = 1;
            else if (scroll_target->y < THIS->y - 1) dy = -1;
        }

        if ((--THIS->custom_data[CD_FRAME_TIMER]) == 0) {
            THIS->custom_data[CD_FRAME_TIMER] = WALK_SPEED;
            frame = (THIS->anim_frame + 1) % TOTAL_FRAMES;
        }
    } else if (state == STATE_WINDUP) {
        if (--THIS->custom_data[CD_CHARGE_TIMER] == 0) {
            THIS->custom_data[CD_CHARGE_STATE] = STATE_DASH;
            THIS->custom_data[CD_CHARGE_TIMER] = DASH_STEPS;
            THIS->custom_data[CD_DASH_DX] = 0;
            THIS->custom_data[CD_DASH_DY] = 0;

            if (scroll_target) {
                INT16 adx = (INT16)scroll_target->x - (INT16)THIS->x;
                INT16 ady = (INT16)scroll_target->y - (INT16)THIS->y;
                if (adx < 0) adx = (INT16)(-adx);
                if (ady < 0) ady = (INT16)(-ady);

                if (adx >= ady) {
                    THIS->custom_data[CD_DASH_DX] =
                        (UINT8)((scroll_target->x >= THIS->x) ? 2 : -2);
                    THIS->custom_data[CD_DASH_DY] = 0;
                } else {
                    THIS->custom_data[CD_DASH_DX] = 0;
                    THIS->custom_data[CD_DASH_DY] =
                        (UINT8)((scroll_target->y >= THIS->y) ? 2 : -2);
                }
            }
        }
        frame = 3;
        SetFrame(THIS, frame);
        return;
    } else {
        /* STATE_DASH */
        if (--THIS->custom_data[CD_CHARGE_TIMER] == 0) {
            THIS->custom_data[CD_CHARGE_STATE] = STATE_CHASE;
            THIS->custom_data[CD_CHARGE_TIMER] = CHARGE_COOLDOWN;
            THIS->custom_data[CD_MOVE_TIMER] = 0;
        } else {
            dx = (INT8)THIS->custom_data[CD_DASH_DX];
            dy = (INT8)THIS->custom_data[CD_DASH_DY];
            if (SafeTranslateSprite(THIS, dx, dy)) {
                /* Hit a wall — cancel dash early */
                THIS->custom_data[CD_CHARGE_STATE] = STATE_CHASE;
                THIS->custom_data[CD_CHARGE_TIMER] = CHARGE_COOLDOWN;
            }
            frame = 3;
            SetFrame(THIS, frame);
            return;
        }
    }

    if (THIS->custom_data[CD_BLINK_TIMER] > 0 &&
        ((THIS->custom_data[CD_BLINK_TIMER] / 2) % 2) == 0) {
        frame = 3;
    }

    SetFrame(THIS, frame);
    if (dx || dy) {
        CustomTranslateSprite(THIS, dx, dy);
    }
}

void DESTROY() {
}
