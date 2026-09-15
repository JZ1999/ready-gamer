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

#define SHOOT_COOLDOWN       100
#define PLAYER_MAX_HEALTH    1
#define INVINCIBILITY_FRAMES 60
#define RESPAWN_INVINCIBILITY_FRAMES 180
#define WALK_ANIM_SPEED      8
/* Boss run only — reuses CD_PLAYER_ELECTRIC slot (unused here). */
#define CD_PENDING_WALL_PUSH CD_PLAYER_ELECTRIC

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

static UINT8 CustomTranslateSprite(Sprite* sprite, INT8 x, INT8 y, UINT8 can_phase_walls) {
    if (can_phase_walls) {
        return BossRunTranslateSpritePhasing(sprite, x, y);
    }
    return BossRunTranslateSprite(sprite, x, y);
}

static UINT8 IsOverlappingSolid(Sprite* sprite, UINT8 interior_only) {
    UINT16 x = sprite->x;
    UINT16 y = sprite->y;
    UINT8 cw = sprite->coll_w;
    UINT8 ch = sprite->coll_h;
    UINT8 left = (UINT8)(x >> 3);
    UINT8 right = (UINT8)((x + cw - 1) >> 3);
    UINT8 top = (UINT8)(y >> 3);
    UINT8 bottom = (UINT8)((y + ch - 1) >> 3);
    UINT8 tile_x;
    UINT8 tile_y;

    for (tile_y = top; tile_y <= bottom; tile_y++) {
        if (interior_only && (tile_y == 0 || tile_y == (UINT8)(BOSSRUN_MAP_TILES_H - 1))) {
            continue;
        }
        for (tile_x = left; tile_x <= right; tile_x++) {
            if (BossRunTileBlocked(tile_x, tile_y)) {
                return 1;
            }
        }
    }

    return 0;
}

static void PushForwardOutOfWall(Sprite* sprite) {
    UINT8 cw = sprite->coll_w ? sprite->coll_w : 1;
    UINT16 limit = (UINT16)scroll_x + SCREENWIDTH - cw;
    UINT8 steps = 0;

    if (limit > BOSSRUN_MAP_PIXELS_W - cw) {
        limit = BOSSRUN_MAP_PIXELS_W - cw;
    }

    while (IsOverlappingSolid(sprite, 1) && sprite->x < limit && steps < 80) {
        sprite->x++;
        steps++;
    }
}

void BossRunTakeDamage(Sprite* player) BANKED {
    if(player->custom_data[CD_INVINCIBILITY] > 0) return;

    if(player->custom_data[CD_PLAYER_HEALTH] > 0) {
        player->custom_data[CD_PLAYER_HEALTH]--;
        PlayBossArenaDamageSound();

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
    THIS->custom_data[CD_PENDING_WALL_PUSH] = 0;

    OBP1_REG = PAL_DEF(0, 0, 0, 0);
    SPRITE_SET_DMG_PALETTE(THIS, 0);

    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    UINT8 moved = 0;
    UINT8 invuln_before = THIS->custom_data[CD_INVINCIBILITY];
    UINT8 can_phase_walls = invuln_before > 0;

    if(boss_run_shoot_cooldown > 0) boss_run_shoot_cooldown--;
    if(THIS->custom_data[CD_INVINCIBILITY] > 0) THIS->custom_data[CD_INVINCIBILITY]--;

    if(THIS->custom_data[CD_INVINCIBILITY] > 0 && (THIS->custom_data[CD_INVINCIBILITY] / 6) % 2 == 0) {
        SPRITE_SET_DMG_PALETTE(THIS, 1);
    } else {
        SPRITE_SET_DMG_PALETTE(THIS, 0);
    }

    if(KEY_PRESSED(J_UP)) {
        if (!CustomTranslateSprite(THIS, 0, -1, can_phase_walls)) moved = 1;
        boss_run_player_direction = DIR_UP;
    }
    if(KEY_PRESSED(J_DOWN)) {
        if (!CustomTranslateSprite(THIS, 0, 1, can_phase_walls)) moved = 1;
        boss_run_player_direction = DIR_DOWN;
    }
    if(KEY_PRESSED(J_LEFT)) {
        if (!CustomTranslateSprite(THIS, -1, 0, can_phase_walls)) moved = 1;
        boss_run_player_direction = DIR_LEFT;
    }
    if(KEY_PRESSED(J_RIGHT)) {
        if (!CustomTranslateSprite(THIS, 1, 0, can_phase_walls)) moved = 1;
        boss_run_player_direction = DIR_RIGHT;
    }

    if (invuln_before > 0 && THIS->custom_data[CD_INVINCIBILITY] == 0 &&
        IsOverlappingSolid(THIS, 1)) {
        THIS->custom_data[CD_PENDING_WALL_PUSH] = 1;
    }

    if (THIS->custom_data[CD_PENDING_WALL_PUSH]) {
        if (IsOverlappingSolid(THIS, 1)) {
            PushForwardOutOfWall(THIS);
        } else {
            THIS->custom_data[CD_PENDING_WALL_PUSH] = 0;
        }
    }

    if (!can_phase_walls && !THIS->custom_data[CD_PENDING_WALL_PUSH] &&
        (INT16)THIS->x < scroll_x) {
        THIS->x = (UINT16)scroll_x;
    }

    if (IsOverlappingSolid(THIS, 0) && !THIS->custom_data[CD_PENDING_WALL_PUSH] &&
        invuln_before == 0 && THIS->custom_data[CD_INVINCIBILITY] == 0) {
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
