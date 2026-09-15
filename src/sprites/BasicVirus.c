#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "SpriteData.h"
#include "BossRun.h"

#define CD_FRAME_TIMER 1

#define ENEMY_SPEED 10
#define TOTAL_FRAMES 3

extern Sprite* scroll_target;

/* Reused in StateBossRun (see UPDATE()/CustomTranslateSprite below) so the
 * auto-scroll level can spawn normal level-1 enemies too — same
 * boss_run_player NULL-guard pattern as SpriteScrew.c/BossBullet.c. NULL in
 * every other state, so none of this changes normal-room behavior. */
extern Sprite* boss_run_player;
void BossRunTakeDamage(Sprite* player) BANKED;

// Custom movement function that checks for partial brick collisions
static UINT8 CustomTranslateSprite(Sprite* sprite, INT8 x, INT8 y) {
    if (boss_run_player) {
        return BossRunTranslateSprite(sprite, x, y);
    }
    // Use the wall avoidance movement function
    extern UINT8 EnemyMoveWithWallAvoidance(Sprite* enemy, INT16 dx, INT16 dy);
    return EnemyMoveWithWallAvoidance(sprite, x, y);
}

void START() {
    THIS->custom_data[CD_FRAME_TIMER] = ENEMY_SPEED;
    THIS->custom_data[CD_ENEMY_HEALTH] = 3;
    THIS->custom_data[CD_MOVE_TIMER] = 0;
    /* Keep alive off-camera (default lim 32 culls on wide maps). */
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    // In StateBossRun, scroll_target is CameraDriver (an invisible sprite
    // that just walks itself forward, see that file) — chasing it directly
    // would mean chasing the camera, not the player. Chase boss_run_player
    // instead whenever it exists; scroll_target is only the right target in
    // the normal room game, where it's the player-follow camera target.
    Sprite* target = boss_run_player ? boss_run_player : scroll_target;

    if (boss_run_player) {
        // Auto-scroll level: no room, nothing to soft-lock on death, so
        // just remove enemies once fully scrolled off the left edge instead
        // of leaving them alive forever off-screen (the 20-sprite pool is
        // shared project-wide — see reference_ready_gamer_sprite_pool_limit).
        if ((INT16)THIS->x + 16 < scroll_x) {
            SpriteManagerRemove(THIS_IDX);
            return;
        }

        // BossRunPlayer.c has no generic per-frame enemy-contact scan (the
        // room game's version of that lives in SpritePlayer.c, which doesn't
        // run in this state) — so, same pattern as BossBullet.c, this enemy
        // checks for and deals its own contact damage.
        if (CheckCollision(THIS, boss_run_player)) {
            BossRunTakeDamage(boss_run_player);
        }
    }

    UINT8* move_timer = &THIS->custom_data[CD_MOVE_TIMER];

    if ((*move_timer)++ < ENEMY_SPEED) return;
    *move_timer = 0;

    UINT16 dx = 0;
    UINT16 dy = 0;

    // Determine movement direction toward player
    if(target->x > THIS->x + 1) dx = 1;
    else if(target->x < THIS->x - 1) dx = -1;
    else if(target->y > THIS->y + 1) dy = 1;
    else if(target->y < THIS->y - 1) dy = -1;

    // Tile collision check
    UINT16 new_x = THIS->x + dx;
    UINT16 new_y = THIS->y + dy;

    UINT8 frame = THIS->anim_frame;

    // Walking animation logic
    if((--THIS->custom_data[CD_FRAME_TIMER]/10) == 0) {
        THIS->custom_data[CD_FRAME_TIMER] = ENEMY_SPEED;
        frame = (THIS->anim_frame + 1) % TOTAL_FRAMES;
    }

    // Hit feedback logic: show hit frame when recently hit
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
    // Cleanup if needed
}
