#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "BossFight.h"

/*
 * Boss's ranged attack for StateBossFight — aimed at the player's position
 * at fire time, travels in a fixed straight line (8-directional snap, no
 * runtime trig: see Boss.c's own FacingTowards for the same axis-comparison
 * idea). Reuses the `bomb` graphic like the corridor's BossBullet.c — this
 * is a different sprite type so that file stays untouched.
 *
 * Closest-approach detection: every frame this tracks the Manhattan
 * distance to the player. If a real hit hasn't landed and that distance
 * starts increasing again, the bullet just passed its closest point to the
 * player — no minimum-closeness gate, so this fires on essentially every
 * shot, not just tight near-misses. When it happens the bullet hands off
 * to its firing boss (BossBeginSwordAttack) instead of just despawning:
 * the boss teleports to that closest-approach position and starts its
 * sword-swing sequence, landing as near the player as the shot ever got
 * even when that's still short of actual sword range.
 *
 * The owner is looked up by slot index (boss_slots[]) rather than a stored
 * pointer, since custom_data can't hold one — but a slot index alone isn't
 * enough: if the firing boss dies while this bullet is still in flight (its
 * own BULLET_WAIT_FRAMES window covers that), its slot could already be
 * reused by a different Boss (e.g. a phase-2 split copy) by the time this
 * bullet resolves, which would otherwise hijack the wrong boss into the
 * sword sequence. Caching the owner's unique_id at spawn and re-checking it
 * against whatever now occupies that slot closes that — a stale match just
 * silently skips the swap instead of acting on it.
 */

#define CD_OWNER_SLOT    0 // set by SpriteManagerAddEx's param before START()
#define CD_STEP_X        1
#define CD_STEP_Y        2
#define CD_PREV_DIST_LO  3
#define CD_PREV_DIST_HI  4
#define CD_BULLET_LIFETIME 5
#define CD_OWNER_UID_LO  6
#define CD_OWNER_UID_HI  7

#define BOSSBULLET_AIMED_SPEED 1   // px/frame per moving axis — tune to taste
#define BOSSBULLET_AIMED_LIFETIME 180 // ~3s safety net, same role as BossBullet's
#define MIN_FLIGHT_FRAMES 10 // don't allow the closest-approach swing trigger
                              // before the shot has actually traveled a bit —
                              // the boss now keeps chasing while it fires, so
                              // it can be right next to the player at the
                              // exact moment it shoots; without this the very
                              // first frame of flight can already be the
                              // closest point, triggering an "instant" swing

extern Sprite* boss_fight_player;
extern Sprite* boss_slots[2];
void BossFightTakeDamage(Sprite* player) BANKED;
void BossBeginSwordAttack(Sprite* boss, UINT16 x, UINT16 y) BANKED;

static INT8 Sign(INT16 v) {
    if (v > 0) return 1;
    if (v < 0) return -1;
    return 0;
}

void START() {
    // custom_data[CD_OWNER_SLOT] (== custom_data[0]) is already set by
    // SpriteManagerAddEx's param injection before START() runs.
    INT16 dx = 0, dy = 0, adx, ady;
    INT8 speed;
    Sprite* owner = boss_slots[THIS->custom_data[CD_OWNER_SLOT]];
    UINT16 owner_uid = owner ? owner->unique_id : 0;
    THIS->custom_data[CD_OWNER_UID_LO] = (UINT8)(owner_uid & 0xFF);
    THIS->custom_data[CD_OWNER_UID_HI] = (UINT8)(owner_uid >> 8);

    // BOSS_VARIANT_FAST_BULLET's whole gimmick — folded straight into the
    // step values below since custom_data has no free slot left for a
    // separate speed field.
    speed = (owner && owner->custom_data[CD_BOSS_VARIANT] == BOSS_VARIANT_FAST_BULLET) ? 2 : 1;

    if (boss_fight_player) {
        dx = (INT16)boss_fight_player->x - (INT16)THIS->x;
        dy = (INT16)boss_fight_player->y - (INT16)THIS->y;
    }
    adx = dx < 0 ? -dx : dx;
    ady = dy < 0 ? -dy : dy;

    if (adx > (INT16)(ady * 2)) {
        THIS->custom_data[CD_STEP_X] = (UINT8)(Sign(dx) * speed);
        THIS->custom_data[CD_STEP_Y] = 0;
    } else if (ady > (INT16)(adx * 2)) {
        THIS->custom_data[CD_STEP_X] = 0;
        THIS->custom_data[CD_STEP_Y] = (UINT8)(Sign(dy) * speed);
    } else {
        THIS->custom_data[CD_STEP_X] = (UINT8)(Sign(dx) * speed);
        THIS->custom_data[CD_STEP_Y] = (UINT8)(Sign(dy) * speed);
    }

    THIS->custom_data[CD_PREV_DIST_LO] = 0xFF;
    THIS->custom_data[CD_PREV_DIST_HI] = 0xFF; // first-frame distance can never exceed this, so the near-miss check can't false-fire on frame 1
    THIS->custom_data[CD_BULLET_LIFETIME] = BOSSBULLET_AIMED_LIFETIME;

    THIS->lim_x = 255;
    THIS->lim_y = 255;
    SetFrame(THIS, 0);
}

void UPDATE() {
    INT16 dxp, dyp, adxp, adyp;
    UINT16 dist, prev_dist;

    THIS->x = (UINT16)((INT16)THIS->x + (INT8)THIS->custom_data[CD_STEP_X] * BOSSBULLET_AIMED_SPEED);
    THIS->y = (UINT16)((INT16)THIS->y + (INT8)THIS->custom_data[CD_STEP_Y] * BOSSBULLET_AIMED_SPEED);

    if ((INT16)THIS->x < -16 || (INT16)THIS->x > 176 || (INT16)THIS->y < -16 || (INT16)THIS->y > 160) {
        SpriteManagerRemove(THIS_IDX);
        return;
    }

    if (boss_fight_player && CheckCollision(THIS, boss_fight_player)) {
        BossFightTakeDamage(boss_fight_player);
        SpriteManagerRemove(THIS_IDX);
        return;
    }

    if (boss_fight_player) {
        dxp = (INT16)THIS->x - (INT16)boss_fight_player->x;
        dyp = (INT16)THIS->y - (INT16)boss_fight_player->y;
        adxp = dxp < 0 ? -dxp : dxp;
        adyp = dyp < 0 ? -dyp : dyp;
        dist = (UINT16)(adxp + adyp);
        prev_dist = (UINT16)THIS->custom_data[CD_PREV_DIST_LO] | ((UINT16)THIS->custom_data[CD_PREV_DIST_HI] << 8);

        /* No distance gate: teleport to the closest-approach point every
         * time (not just tight near-misses), so the boss always ends up
         * as close to the player as this shot ever got, even if that's
         * still short of actual sword range. Still requires MIN_FLIGHT_FRAMES
         * of actual travel first — see that define. */
        if (dist > prev_dist &&
            (BOSSBULLET_AIMED_LIFETIME - THIS->custom_data[CD_BULLET_LIFETIME]) >= MIN_FLIGHT_FRAMES) {
            Sprite* owner = boss_slots[THIS->custom_data[CD_OWNER_SLOT]];
            UINT16 owner_uid = (UINT16)THIS->custom_data[CD_OWNER_UID_LO] |
                                ((UINT16)THIS->custom_data[CD_OWNER_UID_HI] << 8);
            if (owner && owner->unique_id == owner_uid) {
                BossBeginSwordAttack(owner, THIS->x, THIS->y);
            }
            SpriteManagerRemove(THIS_IDX);
            return;
        }

        THIS->custom_data[CD_PREV_DIST_LO] = (UINT8)(dist & 0xFF);
        THIS->custom_data[CD_PREV_DIST_HI] = (UINT8)(dist >> 8);
    }

    if (--THIS->custom_data[CD_BULLET_LIFETIME] == 0) {
        SpriteManagerRemove(THIS_IDX);
    }
}

void DESTROY() {
}
