#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "SoundEffects.h"
#include "Scroll.h"
#include "Palette.h"
#include "BossRun.h"

/*
 * Same player as the normal game (health/lives/respawn/invincibility
 * flash/shooting) — copied from SpritePlayer.c rather than shared, so a
 * change here can never regress the dungeon-crawler's player. The only
 * real difference: wall collision goes through BossRunTranslateSprite
 * (mapboss's own simplified collision) instead of the room system's
 * SafeTranslateSprite. TakeDamage is renamed to BossRunTakeDamage to
 * avoid a duplicate-symbol clash with SpritePlayer.c's own TakeDamage.
 */

Direction boss_run_player_direction;

extern UINT8 player_lives;

#define BOSSRUN_STARTING_LIVES 2

#define SHOOT_COOLDOWN       100
#define PLAYER_MAX_HEALTH    1
#define INVINCIBILITY_FRAMES 60
#define RESPAWN_INVINCIBILITY_FRAMES 180
#define WALK_ANIM_SPEED      8

#define PLAYER_FRAME_IDLE        0
#define PLAYER_FRAME_UP          1
#define PLAYER_FRAME_DOWN        2
#define PLAYER_FRAME_WALK_DOWN_A 3
#define PLAYER_FRAME_WALK_DOWN_B 4
#define PLAYER_FRAME_WALK_UP_A   5
#define PLAYER_FRAME_WALK_UP_B   6
#define PLAYER_FRAME_WALK_SIDE_A 7
#define PLAYER_FRAME_WALK_SIDE_B 8

Sprite* boss_run_player = NULL;
UINT8 boss_run_shoot_cooldown;

static void SetPlayerIdleFrame(void) {
    switch (boss_run_player_direction) {
        case DIR_UP:
            THIS->mirror = NO_MIRROR;
            SetFrame(THIS, PLAYER_FRAME_UP);
            break;
        case DIR_DOWN:
            THIS->mirror = NO_MIRROR;
            SetFrame(THIS, PLAYER_FRAME_DOWN);
            break;
        case DIR_RIGHT:
            THIS->mirror = V_MIRROR;
            SetFrame(THIS, PLAYER_FRAME_IDLE);
            break;
        default:
            THIS->mirror = NO_MIRROR;
            SetFrame(THIS, PLAYER_FRAME_IDLE);
            break;
    }
}

static void SetPlayerWalkFrame(void) {
    UINT8 step = (THIS->custom_data[CD_WALK_TIMER] / WALK_ANIM_SPEED) & 1;
    UINT8 walk_down_frame = step ? PLAYER_FRAME_WALK_DOWN_B : PLAYER_FRAME_WALK_DOWN_A;
    UINT8 walk_side_frame = step ? PLAYER_FRAME_WALK_SIDE_B : PLAYER_FRAME_WALK_SIDE_A;

    switch (boss_run_player_direction) {
        case DIR_UP:
            THIS->mirror = NO_MIRROR;
            SetFrame(THIS, step ? PLAYER_FRAME_WALK_UP_B : PLAYER_FRAME_WALK_UP_A);
            break;
        case DIR_DOWN:
            THIS->mirror = NO_MIRROR;
            SetFrame(THIS, walk_down_frame);
            break;
        case DIR_RIGHT:
            THIS->mirror = V_MIRROR;
            SetFrame(THIS, walk_side_frame);
            break;
        default:
            THIS->mirror = NO_MIRROR;
            SetFrame(THIS, walk_side_frame);
            break;
    }
}

static UINT8 CustomTranslateSprite(Sprite* sprite, INT8 x, INT8 y) {
    if (x != 0 && sprite->custom_data[CD_INVINCIBILITY] > 0) {
        /* Phase through walls horizontally while invincible — otherwise a
         * player who just got crushed against a wall by the advancing
         * camera (see IsCrushedByWall below) has no way to escape before
         * getting hit again the instant invincibility runs out. Only the
         * x-axis needs this: the camera clamp (UPDATE()) only ever forces
         * x past collision, never y, so vertical movement can keep using
         * normal wall collision even while invincible. */
        sprite->x = (UINT16)((INT16)sprite->x + x);
        return 0;
    }
    return BossRunTranslateSprite(sprite, x, y);
}

/* True if the sprite's current position overlaps a wall tile. Normal
 * movement collision (BossRunTranslateSprite) never lets this happen —
 * the only way to end up here is the scroll_x camera-clamp in UPDATE()
 * forcing the player's x without a collision check, pinning them into a
 * pillar that happens to be sitting right at the camera's edge. */
static UINT8 IsCrushedByWall(Sprite* sprite) {
    UINT16 x = sprite->x;
    UINT16 y = sprite->y;
    UINT8 cw = sprite->coll_w;
    UINT8 ch = sprite->coll_h;
    UINT8 left = (UINT8)(x >> 3);
    UINT8 right = (UINT8)((x + cw - 1) >> 3);
    UINT8 top = (UINT8)(y >> 3);
    UINT8 bottom = (UINT8)((y + ch - 1) >> 3);

    return BossRunTileBlocked(left, top) || BossRunTileBlocked(right, top) ||
           BossRunTileBlocked(left, bottom) || BossRunTileBlocked(right, bottom);
}

void BossRunTakeDamage(Sprite* player) BANKED {
    if(player->custom_data[CD_INVINCIBILITY] > 0) return;

    if(player->custom_data[CD_PLAYER_HEALTH] > 0) {
        player->custom_data[CD_PLAYER_HEALTH]--;
        PlayPlayerHitSound();

        if(player->custom_data[CD_PLAYER_HEALTH] == 0) {
            if(player_lives > 0) {
                player_lives--;
                player->custom_data[CD_PLAYER_HEALTH] = PLAYER_MAX_HEALTH;
                player->custom_data[CD_INVINCIBILITY] = RESPAWN_INVINCIBILITY_FRAMES;
            } else {
                UINT8 i;
                Sprite* spr;
                SPRITEMANAGER_ITERATE(i, spr) {
                    if (spr->unique_id == player->unique_id) {
                        SpriteManagerRemove(i);
                        SetState(StateGameOver);
                        break;
                    }
                }
            }
        } else {
            player->custom_data[CD_INVINCIBILITY] = INVINCIBILITY_FRAMES;
        }
    }
}

void START() {
    SetFrame(THIS, 0);
    boss_run_player_direction = DIR_LEFT;
    boss_run_shoot_cooldown = 0;
    boss_run_player = THIS;

    THIS->custom_data[CD_PLAYER_HEALTH] = PLAYER_MAX_HEALTH;
    THIS->custom_data[CD_INVINCIBILITY] = 0;
    THIS->custom_data[CD_WALK_TIMER] = 0;

    player_lives = BOSSRUN_STARTING_LIVES;

    OBP1_REG = PAL_DEF(0, 0, 0, 0);
    SPRITE_SET_DMG_PALETTE(THIS, 0);

    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    UINT8 moved = 0;

    if(boss_run_shoot_cooldown > 0) boss_run_shoot_cooldown--;
    if(THIS->custom_data[CD_INVINCIBILITY] > 0) THIS->custom_data[CD_INVINCIBILITY]--;

    if(THIS->custom_data[CD_INVINCIBILITY] > 0 && (THIS->custom_data[CD_INVINCIBILITY] / 6) % 2 == 0) {
        SPRITE_SET_DMG_PALETTE(THIS, 1);
    } else {
        SPRITE_SET_DMG_PALETTE(THIS, 0);
    }

    if(KEY_PRESSED(J_UP)) {
        if (!CustomTranslateSprite(THIS, 0, -1)) moved = 1;
        boss_run_player_direction = DIR_UP;
    }
    if(KEY_PRESSED(J_DOWN)) {
        if (!CustomTranslateSprite(THIS, 0, 1)) moved = 1;
        boss_run_player_direction = DIR_DOWN;
    }
    if(KEY_PRESSED(J_LEFT)) {
        if (!CustomTranslateSprite(THIS, -1, 0)) moved = 1;
        boss_run_player_direction = DIR_LEFT;
    }
    if(KEY_PRESSED(J_RIGHT)) {
        if (!CustomTranslateSprite(THIS, 1, 0)) moved = 1;
        boss_run_player_direction = DIR_RIGHT;
    }

    /* The camera auto-scrolls whether the player keeps up or not (see
     * CameraDriver.c) — without this, standing still just lets the level
     * scroll away and leave the player stranded off-screen-left, forever
     * out of any bullet's reach. Clamp to the camera's left edge, same
     * role a moving wall plays in any auto-scroller. */
    if ((INT16)THIS->x < scroll_x) {
        THIS->x = (UINT16)scroll_x;
    }

    /* Crushed between the camera and a wall — take a hit (no-ops if
     * already invincible, so this is safe to check every frame). While
     * invincible, CustomTranslateSprite above lets the player move
     * through walls to actually get clear before it happens again. */
    if (IsCrushedByWall(THIS)) {
        BossRunTakeDamage(THIS);
    }

    if (moved) {
        THIS->custom_data[CD_WALK_TIMER]++;
        SetPlayerWalkFrame();
    } else {
        SetPlayerIdleFrame();
    }

    if(KEY_PRESSED(J_B) && boss_run_shoot_cooldown == 0) {
        Sprite* projectile = SpriteManagerAddEx(SpriteScrew, THIS->x, THIS->y, (UINT8)boss_run_player_direction);
        projectile->custom_data[CD_DIR] = (UINT8)boss_run_player_direction;
        boss_run_shoot_cooldown = SHOOT_COOLDOWN;
        PlayScrewShotSound();
    }
}

void DESTROY() {
    boss_run_player = NULL;
}
