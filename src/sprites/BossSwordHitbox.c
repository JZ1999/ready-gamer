#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"

/*
 * Boss's melee punish, spawned by Boss.c at the end of the sword-telegraph
 * state (after the near-miss teleport swap). Fixed position beside the
 * boss on the side facing the player (no sliding, no boss animation) —
 * plays slashGfx.c's 3-frame rotating blade arc for that direction
 * (diagonal -> diagonal -> straight, A Link to the Past style), always
 * pointed at the player.
 *
 * slashGfx.c only stores RIGHT and DOWN (6 frames) — LEFT reuses RIGHT's
 * frames with H_MIRROR, UP reuses DOWN's frames with V_MIRROR, instead of
 * doubling the tile data for all 4 directions (see that file's header for
 * why: sprite-tile VRAM budget was over the 128-tile hardware ceiling
 * otherwise).
 */

// CD_DIR (== 0, from SpriteData.h) is set by SpriteManagerAddEx's param
// before START() runs.
#define CD_HITBOX_LIFETIME 1
#define CD_BASE_FRAME       2 // 0 (RIGHT/LEFT frames) or 3 (DOWN/UP frames)
#define SLASH_FRAME_COUNT   3

#define SWORD_HITBOX_LIFETIME 14 // matches Boss.c's SWORD_ACTIVE_FRAMES
#define SWORD_REACH 16           // ~player-sized reach, both axes

extern Sprite* boss_fight_player;
void BossFightTakeDamage(Sprite* player) BANKED;

void START() {
    UINT8 dir = THIS->custom_data[CD_DIR];
    INT16 boss_x = (INT16)THIS->x;
    INT16 boss_y = (INT16)THIS->y;
    INT16 x, y;
    UINT8 base_frame;

    THIS->coll_w = SWORD_REACH;
    THIS->coll_h = SWORD_REACH;

    THIS->mirror = NO_MIRROR;
    switch (dir) {
        case DIR_UP:
            x = boss_x; // boss is 16px wide, same as SWORD_REACH — no centering needed
            y = boss_y - SWORD_REACH;
            base_frame = SLASH_FRAME_COUNT; // DOWN's frames, flipped vertically
            THIS->mirror = V_MIRROR;
            break;
        case DIR_DOWN:
            x = boss_x;
            y = boss_y + 16; // boss's own height
            base_frame = SLASH_FRAME_COUNT;
            break;
        case DIR_LEFT:
            x = boss_x - SWORD_REACH;
            y = boss_y;
            base_frame = 0; // RIGHT's frames, flipped horizontally
            THIS->mirror = H_MIRROR;
            break;
        default: // DIR_RIGHT
            x = boss_x + 16; // boss's own width
            y = boss_y;
            base_frame = 0;
            break;
    }

    // Clamp instead of letting a near-wall boss underflow x/y (UINT16 wrap
    // would teleport the hitbox miles off-map instead of just against the
    // wall) — same edge the boss's own movement collision already keeps it
    // clear of, but the offsets here can still push a few px past 0.
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    THIS->x = (UINT16)x;
    THIS->y = (UINT16)y;

    THIS->custom_data[CD_BASE_FRAME] = base_frame;
    THIS->custom_data[CD_HITBOX_LIFETIME] = SWORD_HITBOX_LIFETIME;
    THIS->lim_x = 255;
    THIS->lim_y = 255;
    SetFrame(THIS, base_frame);
}

void UPDATE() {
    UINT8 elapsed, step;

    if (boss_fight_player && CheckCollision(THIS, boss_fight_player)) {
        BossFightTakeDamage(boss_fight_player);
        SpriteManagerRemove(THIS_IDX);
        return;
    }

    // 3-frame rotating-blade arc across the hitbox's lifetime.
    elapsed = SWORD_HITBOX_LIFETIME - THIS->custom_data[CD_HITBOX_LIFETIME];
    step = (elapsed * SLASH_FRAME_COUNT) / SWORD_HITBOX_LIFETIME;
    if (step > SLASH_FRAME_COUNT - 1) step = SLASH_FRAME_COUNT - 1;
    SetFrame(THIS, THIS->custom_data[CD_BASE_FRAME] + step);

    if (--THIS->custom_data[CD_HITBOX_LIFETIME] == 0) {
        SpriteManagerRemove(THIS_IDX);
    }
}

void DESTROY() {
}
