#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"

/*
 * Pure spread pressure for BOSS_VARIANT_TRIPLE_SHOT (Boss.c) — a straight
 * 8-directional bullet with no near-miss/teleport logic (that stays only
 * on BossBulletAimed's own player-aimed shot, since that one is almost
 * always going to end up closer to the player than a deliberately-offset
 * diagonal shot anyway — keeping the "teleport to whichever shot got
 * closest" behavior without needing to compare across 3 in-flight bullets
 * at once, which custom_data's 8 slots don't have room for). Reuses the
 * `bomb` graphic like BossBulletAimed.
 *
 * Direction comes packed in custom_data[0] (set by SpriteManagerAddEx's
 * param): (dx+1) + (dy+1)*3, dx/dy each in {-1,0,1} — decoded in START().
 */

#define CD_PACKED_DIR       0 // set by SpriteManagerAddEx's param before START()
#define CD_STEP_X           1
#define CD_STEP_Y           2
#define CD_LIFETIME         3

#define BOSSBULLET_DIAGONAL_SPEED 1
#define BOSSBULLET_DIAGONAL_LIFETIME 180

extern Sprite* boss_fight_player;
void BossFightTakeDamage(Sprite* player) BANKED;

void START() {
    UINT8 packed = THIS->custom_data[CD_PACKED_DIR];

    THIS->custom_data[CD_STEP_X] = (packed % 3); // 0,1,2 -> reinterpreted as signed below
    THIS->custom_data[CD_STEP_Y] = (packed / 3);

    THIS->custom_data[CD_LIFETIME] = BOSSBULLET_DIAGONAL_LIFETIME;
    THIS->lim_x = 255;
    THIS->lim_y = 255;
    SetFrame(THIS, 0);
}

void UPDATE() {
    INT8 step_x = (INT8)THIS->custom_data[CD_STEP_X] - 1;
    INT8 step_y = (INT8)THIS->custom_data[CD_STEP_Y] - 1;

    THIS->x = (UINT16)((INT16)THIS->x + step_x * BOSSBULLET_DIAGONAL_SPEED);
    THIS->y = (UINT16)((INT16)THIS->y + step_y * BOSSBULLET_DIAGONAL_SPEED);

    if ((INT16)THIS->x < -16 || (INT16)THIS->x > 176 || (INT16)THIS->y < -16 || (INT16)THIS->y > 160) {
        SpriteManagerRemove(THIS_IDX);
        return;
    }

    if (boss_fight_player && CheckCollision(THIS, boss_fight_player)) {
        BossFightTakeDamage(boss_fight_player);
        SpriteManagerRemove(THIS_IDX);
        return;
    }

    if (--THIS->custom_data[CD_LIFETIME] == 0) {
        SpriteManagerRemove(THIS_IDX);
    }
}

void DESTROY() {
}
