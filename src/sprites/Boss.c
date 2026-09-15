#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "BossFight.h"
#include "SoundEffects.h"
#include <rand.h>

/*
 * Arena boss (StateBossFight). Phase 1: chase the player diagonally, then
 * fire an aimed shot (BossBulletAimed handles its own near-miss detection
 * and calls BossBeginSwordAttack() back on a real near-miss — see that
 * file). Phase 2: on death, the main boss (not itself a split copy) spawns
 * two weaker, differently-behaved copies (see CD_BOSS_VARIANT in
 * BossFight.h) that each run this exact same state machine independently.
 * StateBossFight's own UPDATE() counts live Boss sprites each frame to
 * know when to trigger StateWin — this file doesn't need to track that
 * itself.
 *
 * Split-copy asymmetry (was: both copies ran identically, making the
 * split trivial — same pattern, easy to dodge both at once):
 *   BOSS_VARIANT_FAST_BULLET — shoots faster, moves slower.
 *   BOSS_VARIANT_TRIPLE_SHOT — fires 1 aimed shot + 2 diagonal spread
 *     shots instead of 1. Only the aimed shot (BossBulletAimed) does
 *     near-miss/teleport detection; the 2 diagonal ones (BossBulletDiagonal)
 *     are pure pressure. This also answers "teleport to whichever bullet
 *     is closest" without needing to compare 3 in-flight bullets against
 *     each other (no room left in custom_data for that) — the aimed shot
 *     is deliberately always the one flying at the player, so it's always
 *     going to be the closest of the three anyway.
 * Movement also gets a random skip chance now (MOVE_SKIP_CHANCE) so the
 * two copies drift out of lockstep instead of moving in perfect unison.
 */

#define CD_BOSS_HEALTH     0
/* CD_BOSS_STATE(1)/CD_BOSS_TIMER(2) come from BossFight.h — shared with
 * BossBeginSwordAttack, which lives in BossFightCollision.c now. */
#define CD_BOSS_IS_SPLIT   3
#define CD_BOSS_MOVE_TIMER 4
#define CD_BOSS_SLOT       5
/* Per-sprite (not file-static!) — up to 2 bosses run this file's code at
 * once during the phase-2 split, so a plain static counter here would be
 * shared/wrong between them. Throttles the move sound (see ChaseStep) so
 * it doesn't fire on literally every step and turn into a constant chirp. */
#define CD_BOSS_MOVE_SOUND_COUNTER 7

#define STATE_CHASE            0
#define STATE_BULLET_ACTIVE    1
/* STATE_SWORD_TELEGRAPH(2) comes from BossFight.h, same reason as above. */
#define STATE_SWORD_ACTIVE     3
#define STATE_COOLDOWN         4

#define BOSS_MAIN_HP  20
#define BOSS_SPLIT_HP 7

#define MOVE_INTERVAL         4   // frames between each diagonal step — tune to taste
#define MOVE_INTERVAL_SLOW     7  // BOSS_VARIANT_FAST_BULLET's own move interval — slower
#define MOVE_SKIP_CHANCE      60  // out of 255 (~23%) — random.org-style jitter so split
                                   // copies don't move in perfect lockstep
#define MOVE_SOUND_EVERY_N_STEPS 3 // play the move sound on 1 in N actual steps —
                                    // every step (up to ~15/sec at MOVE_INTERVAL) was a
                                    // constant chirp, not a "the boss just moved" cue
#define CHASE_DURATION        150 // ~2.5s of chasing before firing — tune to taste
#define BULLET_WAIT_FRAMES    200 // >= BossBulletAimed's own lifetime, so the boss
                                   // doesn't move again while its shot is still live
#define SWORD_ACTIVE_FRAMES    14
#define POST_ATTACK_COOLDOWN   20

#define SPLIT_OFFSET 14

/* Up to 2 live bosses at once (phase 2). BossBulletAimed looks its firing
 * owner up here by slot index instead of storing a raw pointer in its own
 * (very limited) custom_data. */
Sprite* boss_slots[2];

extern Sprite* boss_fight_player;

/* Only x needs clamping for the split spawn below — the dying boss's own y
 * is already guaranteed valid (it got there via normal collision-checked
 * chase movement), it's just the +/- horizontal offset that can push a
 * spawn past the arena wall. */
static UINT16 ClampX(INT16 x) {
    if (x < 12) return 12;
    if (x > BOSSFIGHT_MAP_PIXELS_W - 12 - 16) return BOSSFIGHT_MAP_PIXELS_W - 12 - 16;
    return (UINT16)x;
}

/* Diagonal step towards the player, gated by CD_BOSS_MOVE_TIMER. Shared by
 * STATE_CHASE and STATE_BULLET_ACTIVE so the boss keeps closing in even
 * while its shot is still live, instead of standing still after firing.
 * BOSS_VARIANT_FAST_BULLET uses a longer interval (moves slower, to
 * balance its faster shots) and every tick has a random chance to skip
 * entirely, so the two split copies stop moving in perfect lockstep. */
static void ChaseStep(void) {
    UINT8 interval = (THIS->custom_data[CD_BOSS_VARIANT] == BOSS_VARIANT_FAST_BULLET)
                      ? MOVE_INTERVAL_SLOW : MOVE_INTERVAL;

    if (boss_fight_player && (++THIS->custom_data[CD_BOSS_MOVE_TIMER] >= interval)) {
        INT8 dx = 0, dy = 0;
        THIS->custom_data[CD_BOSS_MOVE_TIMER] = 0;

        if (rand() < MOVE_SKIP_CHANCE) return;

        if (boss_fight_player->x > THIS->x) dx = 1;
        else if (boss_fight_player->x < THIS->x) dx = -1;
        if (boss_fight_player->y > THIS->y) dy = 1;
        else if (boss_fight_player->y < THIS->y) dy = -1;

        if (dx || dy) {
            BossFightTranslateSprite(THIS, dx, dy);
            if (++THIS->custom_data[CD_BOSS_MOVE_SOUND_COUNTER] >= MOVE_SOUND_EVERY_N_STEPS) {
                THIS->custom_data[CD_BOSS_MOVE_SOUND_COUNTER] = 0;
                PlayBossMoveSound();
            }
        }
    }
}

static INT8 Sign(INT16 v) {
    if (v > 0) return 1;
    if (v < 0) return -1;
    return 0;
}

/* 8-direction compass, clockwise from RIGHT — used only to find the two
 * neighbors of the aimed shot's own direction for BOSS_VARIANT_TRIPLE_SHOT's
 * diagonal spread. */
static const INT8 COMPASS_DX[8] = { 1, 1, 0, -1, -1, -1, 0, 1 };
static const INT8 COMPASS_DY[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };

static UINT8 CompassIndex(INT8 sx, INT8 sy) {
    UINT8 i;
    for (i = 0; i != 8; ++i) {
        if (COMPASS_DX[i] == sx && COMPASS_DY[i] == sy) return i;
    }
    return 0; // sx==sy==0 (boss and player exactly overlapping) — arbitrary, won't matter
}

/* Same axis-snap ratio BossBulletAimed's own START() uses, duplicated here
 * so the two BOSS_VARIANT_TRIPLE_SHOT diagonal shots can be picked as the
 * aimed shot's compass neighbors — they need to agree on what "aimed"
 * means without the two files calling into each other for it. */
static void SnapAim(INT16 dx, INT16 dy, INT8* sx, INT8* sy) {
    INT16 adx = dx < 0 ? -dx : dx;
    INT16 ady = dy < 0 ? -dy : dy;

    if (adx > (INT16)(ady * 2)) {
        *sx = Sign(dx); *sy = 0;
    } else if (ady > (INT16)(adx * 2)) {
        *sx = 0; *sy = Sign(dy);
    } else {
        *sx = Sign(dx); *sy = Sign(dy);
    }
}

static void FireAtPlayer(void) {
    UINT8 slot = THIS->custom_data[CD_BOSS_SLOT];

    PlayBossShootSound();
    SpriteManagerAddEx(BossBulletAimed, THIS->x, THIS->y, slot);

    if (THIS->custom_data[CD_BOSS_VARIANT] == BOSS_VARIANT_TRIPLE_SHOT) {
        INT8 sx, sy;
        UINT8 idx, i;
        PlayBossSpreadShotSound();
        SnapAim((INT16)boss_fight_player->x - (INT16)THIS->x,
                (INT16)boss_fight_player->y - (INT16)THIS->y, &sx, &sy);
        idx = CompassIndex(sx, sy);
        for (i = 0; i != 2; ++i) {
            UINT8 nidx = i == 0 ? (UINT8)((idx + 7) % 8) : (UINT8)((idx + 1) % 8);
            UINT8 packed = (UINT8)(COMPASS_DX[nidx] + 1) + (UINT8)(COMPASS_DY[nidx] + 1) * 3;
            SpriteManagerAddEx(BossBulletDiagonal, THIS->x, THIS->y, packed);
        }
    }
}

static Direction FacingTowards(Sprite* from, Sprite* to) {
    INT16 dx = (INT16)to->x - (INT16)from->x;
    INT16 dy = (INT16)to->y - (INT16)from->y;
    INT16 adx = dx < 0 ? -dx : dx;
    INT16 ady = dy < 0 ? -dy : dy;

    if (adx >= ady) {
        return dx >= 0 ? DIR_RIGHT : DIR_LEFT;
    }
    return dy >= 0 ? DIR_DOWN : DIR_UP;
}

static void HandleDeath(void) {
    UINT8 slot = THIS->custom_data[CD_BOSS_SLOT];
    UINT8 was_split = THIS->custom_data[CD_BOSS_IS_SPLIT];
    UINT16 x = THIS->x;
    UINT16 y = THIS->y;

    boss_slots[slot] = NULL;
    PlayBossDefeatMelody();

    if (!was_split) {
        Sprite* childA = SpriteManagerAddEx(Boss, ClampX((INT16)x - SPLIT_OFFSET), y, BOSS_SPLIT_HP);
        Sprite* childB = SpriteManagerAddEx(Boss, ClampX((INT16)x + SPLIT_OFFSET), y, BOSS_SPLIT_HP);
        // Asymmetric split — see file header comment. Set after spawn since
        // SpriteManagerAddEx's own param slot is already used for HP.
        childA->custom_data[CD_BOSS_VARIANT] = BOSS_VARIANT_FAST_BULLET;
        childB->custom_data[CD_BOSS_VARIANT] = BOSS_VARIANT_TRIPLE_SHOT;
    }

    SpriteManagerRemove(THIS_IDX);
}

void START() {
    UINT8 slot;

    /* custom_data[CD_BOSS_HEALTH] (== custom_data[0]) is already set by
     * SpriteManagerAddEx's param injection before START() runs. */
    THIS->custom_data[CD_BOSS_IS_SPLIT] = (THIS->custom_data[CD_BOSS_HEALTH] == BOSS_MAIN_HP) ? 0 : 1;
    THIS->custom_data[CD_BOSS_STATE] = STATE_CHASE;
    THIS->custom_data[CD_BOSS_TIMER] = CHASE_DURATION;
    THIS->custom_data[CD_BOSS_MOVE_TIMER] = 0;
    THIS->custom_data[CD_BOSS_MOVE_SOUND_COUNTER] = 0;
    THIS->custom_data[CD_BOSS_VARIANT] = BOSS_VARIANT_NORMAL; // HandleDeath overrides this on split children

    slot = (boss_slots[0] == NULL) ? 0 : 1;
    boss_slots[slot] = THIS;
    THIS->custom_data[CD_BOSS_SLOT] = slot;

    /* coll_w/coll_h (16x16) already set by InitSprite from bossGfx.c's
     * MetaSpriteInfo width/height. */
    THIS->lim_x = 255;
    THIS->lim_y = 255;
    SetFrame(THIS, 0);
}

void UPDATE() {
    UINT8 state;

    if (THIS->custom_data[CD_BOSS_HEALTH] == 0) {
        HandleDeath();
        return;
    }

    state = THIS->custom_data[CD_BOSS_STATE];

    switch (state) {
        case STATE_CHASE: {
            SetFrame(THIS, 0);
            ChaseStep();

            if (--THIS->custom_data[CD_BOSS_TIMER] == 0) {
                if (boss_fight_player) {
                    FireAtPlayer();
                }
                THIS->custom_data[CD_BOSS_STATE] = STATE_BULLET_ACTIVE;
                THIS->custom_data[CD_BOSS_TIMER] = BULLET_WAIT_FRAMES;
            }
            break;
        }

        case STATE_BULLET_ACTIVE: {
            SetFrame(THIS, 1);
            /* Keep closing the distance while the shot is still live —
             * only the telegraph/sword-swing states actually root the
             * boss in place. */
            ChaseStep();

            if (--THIS->custom_data[CD_BOSS_TIMER] == 0) {
                THIS->custom_data[CD_BOSS_STATE] = STATE_COOLDOWN;
                THIS->custom_data[CD_BOSS_TIMER] = POST_ATTACK_COOLDOWN;
            }
            break;
        }

        case STATE_SWORD_TELEGRAPH: {
            SetFrame(THIS, ((THIS->custom_data[CD_BOSS_TIMER] / 3) & 1) ? 1 : 0);
            if (--THIS->custom_data[CD_BOSS_TIMER] == 0) {
                if (boss_fight_player) {
                    Direction facing = FacingTowards(THIS, boss_fight_player);
                    Sprite* hitbox = SpriteManagerAddEx(BossSwordHitbox, THIS->x, THIS->y, (UINT8)facing);
                    hitbox->custom_data[CD_DIR] = (UINT8)facing;
                    PlayBossSwordSound();
                }
                THIS->custom_data[CD_BOSS_STATE] = STATE_SWORD_ACTIVE;
                THIS->custom_data[CD_BOSS_TIMER] = SWORD_ACTIVE_FRAMES;
            }
            break;
        }

        case STATE_SWORD_ACTIVE: {
            SetFrame(THIS, 1);
            if (--THIS->custom_data[CD_BOSS_TIMER] == 0) {
                THIS->custom_data[CD_BOSS_STATE] = STATE_COOLDOWN;
                THIS->custom_data[CD_BOSS_TIMER] = POST_ATTACK_COOLDOWN;
            }
            break;
        }

        default: { /* STATE_COOLDOWN */
            SetFrame(THIS, 0);
            if (--THIS->custom_data[CD_BOSS_TIMER] == 0) {
                THIS->custom_data[CD_BOSS_STATE] = STATE_CHASE;
                THIS->custom_data[CD_BOSS_TIMER] = CHASE_DURATION;
            }
            break;
        }
    }

    if (boss_fight_player && CheckCollision(THIS, boss_fight_player)) {
        BossFightTakeDamage(boss_fight_player);
    }
}

void DESTROY() {
    UINT8 slot = THIS->custom_data[CD_BOSS_SLOT];
    if (boss_slots[slot] == THIS) {
        boss_slots[slot] = NULL;
    }
}
