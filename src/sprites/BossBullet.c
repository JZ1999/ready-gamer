#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"

/* Boss "attack" for StateBossRun: spawned off-screen-right by StateBossRun's
 * timer, travels left at a fixed speed, ends the run on touching the player.
 * No boss body sprite exists (see SESSION_NOTES.md) — the boss is implied,
 * not drawn, to stay within the engine's 20-sprite budget. */

#define CD_LIFETIME 0

#define BOSSBULLET_SPEED    2   // px/frame, moving left
#define BOSSBULLET_LIFETIME 180 // ~3s safety net — normally removed sooner (below)

extern Sprite* boss_run_player;
void BossRunTakeDamage(Sprite* player) BANKED;

void START() {
    THIS->custom_data[CD_LIFETIME] = BOSSBULLET_LIFETIME;
    THIS->lim_x = 255;
    THIS->lim_y = 255;
    SetFrame(THIS, 0);
}

void UPDATE() {
    THIS->x -= BOSSBULLET_SPEED;

    /* Underflow safety net ONLY — an absolute check, not scroll_x-relative.
     * The bug this replaces: comparing against `scroll_x - 16` pruned the
     * bullet right around wherever the player naturally sits in an
     * auto-scroller (near the camera's left edge, keeping pace with it),
     * deleting it before the collision check below ever ran — bullets
     * could never actually hit the player. The lifetime timer further
     * down is the real cleanup mechanism; this only matters in the first
     * couple seconds of the level when x is still close to 0. */
    if (THIS->x < 32) {
        SpriteManagerRemove(THIS_IDX);
        return;
    }

    if (boss_run_player && CheckCollision(THIS, boss_run_player)) {
        BossRunTakeDamage(boss_run_player);
        SpriteManagerRemove(THIS_IDX);
        return;
    }

    if (--THIS->custom_data[CD_LIFETIME] == 0) {
        SpriteManagerRemove(THIS_IDX);
    }
}

void DESTROY() {
}
