#include "Banks/SetAutoBank.h"
#include <rand.h>
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "EnemyUnstick.h"

// Movement ticks between progress checks (Tank moves every 20 frames, so
// 8 ticks ~ 2.7 s; faster enemies check sooner in real time).
#define STUCK_WINDOW_TICKS 8
// Less than this many px (|dx|+|dy|) from the last check = stuck.
#define STUCK_MIN_PROGRESS 3
// Closer than this (px, each axis) to the player = touching/attacking, not
// stuck: don't wander away from the player.
#define NEAR_PLAYER_PX 20
// Escape length in movement ticks: 16 = 2 tiles, one corridor width.
#define ESCAPE_TICKS 16

// Per pool slot (enemies have no free custom_data left: Charge uses all 8).
typedef struct {
    UINT8 anchor_x, anchor_y; // position (low byte) at the last progress check
    UINT8 last_x, last_y;     // position (low byte) at the previous tick
    UINT8 ticks;              // ticks since the last progress check
    INT8 esc_dx, esc_dy;      // escape direction while escaping
    UINT8 esc_left;           // escape ticks left (0 = chasing normally)
} UnstickState;

static UnstickState unstick[N_SPRITE_MANAGER_SPRITES];

extern Sprite* scroll_target;

// Pool slot of a sprite. Returns N_SPRITE_MANAGER_SPRITES if not found
// (should not happen for a live sprite); callers then skip the logic.
static UINT8 SlotOf(Sprite* enemy) {
    UINT8 i;
    for (i = 0; i != N_SPRITE_MANAGER_SPRITES; ++i) {
        if (sprite_manager_sprites[i] == enemy) return i;
    }
    return N_SPRITE_MANAGER_SPRITES;
}

static UINT8 AbsDiff8(UINT8 a, UINT8 b) {
    // Low bytes wrap at 256 px; any real move per window is far smaller,
    // so the shorter way around is the true distance.
    UINT8 d = a - b;
    return (d > 128) ? (UINT8)(0 - d) : d;
}

static UINT8 NearPlayer(Sprite* enemy) {
    INT16 ddx, ddy;
    if (!scroll_target) return 0;
    ddx = (INT16)scroll_target->x - (INT16)enemy->x;
    ddy = (INT16)scroll_target->y - (INT16)enemy->y;
    if (ddx < 0) ddx = -ddx;
    if (ddy < 0) ddy = -ddy;
    return ddx < NEAR_PLAYER_PX && ddy < NEAR_PLAYER_PX;
}

static void SetAnchor(UnstickState* s, Sprite* enemy) {
    s->anchor_x = (UINT8)enemy->x;
    s->anchor_y = (UINT8)enemy->y;
    s->ticks = 0;
}

// Sideways relative to the attempted chase step: if it was pushing on Y,
// slide on X, and vice versa. Random sign; no attempted step = random axis.
static void StartEscape(UnstickState* s, INT8 dx, INT8 dy) {
    INT8 sign = (rand() & 1) ? 1 : -1;
    UINT8 vertical;
    if (dy && !dx) {
        vertical = 0;
    } else if (dx && !dy) {
        vertical = 1;
    } else {
        vertical = rand() & 1;
    }
    s->esc_dx = vertical ? 0 : sign;
    s->esc_dy = vertical ? sign : 0;
    s->esc_left = ESCAPE_TICKS;
}

void EnemyUnstickReset(Sprite* enemy) BANKED {
    UINT8 slot = SlotOf(enemy);
    UnstickState* s;
    if (slot == N_SPRITE_MANAGER_SPRITES) return;
    s = &unstick[slot];
    SetAnchor(s, enemy);
    s->last_x = (UINT8)enemy->x;
    s->last_y = (UINT8)enemy->y;
    s->esc_left = 0;
}

void EnemyUnstickSteer(Sprite* enemy, INT8* dx, INT8* dy) BANKED {
    UINT8 slot = SlotOf(enemy);
    UnstickState* s;
    UINT8 moved_last_tick;
    if (slot == N_SPRITE_MANAGER_SPRITES) return;
    s = &unstick[slot];

    moved_last_tick = ((UINT8)enemy->x != s->last_x) || ((UINT8)enemy->y != s->last_y);
    s->last_x = (UINT8)enemy->x;
    s->last_y = (UINT8)enemy->y;

    if (s->esc_left) {
        // Escaping. Previous escape step hit a wall: reverse along the same
        // axis instead of grinding into it.
        if (!moved_last_tick) {
            s->esc_dx = -s->esc_dx;
            s->esc_dy = -s->esc_dy;
        }
        *dx = s->esc_dx;
        *dy = s->esc_dy;
        if (--s->esc_left == 0) {
            SetAnchor(s, enemy); // fresh window once the chase resumes
        }
        return;
    }

    if (NearPlayer(enemy)) {
        SetAnchor(s, enemy);
        return;
    }

    if (++s->ticks < STUCK_WINDOW_TICKS) return;

    if (AbsDiff8((UINT8)enemy->x, s->anchor_x) + AbsDiff8((UINT8)enemy->y, s->anchor_y) < STUCK_MIN_PROGRESS) {
        StartEscape(s, *dx, *dy);
        *dx = s->esc_dx;
        *dy = s->esc_dy;
        s->esc_left--;
    }
    SetAnchor(s, enemy);
}
