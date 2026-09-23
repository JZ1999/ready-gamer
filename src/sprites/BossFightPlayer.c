#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "SpriteBudget.h"
#include "SoundEffects.h"
#include "Scroll.h"
#include "Palette.h"
#include "BossFight.h"

/*
 * Player for StateBossFight (the static arena after the boss-run corridor).
 * Copy of BossRunPlayer.c, same reasoning as that file's own header comment:
 * a dedicated copy so a change here can never regress the corridor or the
 * room game. Wall collision goes through BossFightTranslateSprite (the
 * arena's own simplified collision) instead of BossRun's. Unlike
 * BossRunPlayer, there's no camera-clamp-to-scroll_x or IsCrushedByWall —
 * both existed only to handle the auto-scroll camera pinning the player
 * against a wall, which can't happen here (the arena never scrolls).
 * TakeDamage is renamed BossFightTakeDamage, same duplicate-symbol reason as
 * BossRunTakeDamage.
 */

Direction boss_fight_player_direction;

extern UINT8 player_lives;

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

Sprite* boss_fight_player = NULL;
UINT8 boss_fight_shoot_cooldown;

static void SetPlayerIdleFrame(void) {
    switch (boss_fight_player_direction) {
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

    switch (boss_fight_player_direction) {
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

void START() {
    SetFrame(THIS, 0);
    boss_fight_player_direction = DIR_LEFT;
    boss_fight_shoot_cooldown = 0;
    boss_fight_player = THIS;

    THIS->custom_data[CD_PLAYER_HEALTH] = PLAYER_MAX_HEALTH;
    THIS->custom_data[CD_INVINCIBILITY] = 0;
    THIS->custom_data[CD_WALK_TIMER] = 0;

    OBP1_REG = PAL_DEF(0, 0, 0, 0);
    SPRITE_SET_DMG_PALETTE(THIS, 0);

    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
    UINT8 moved = 0;

    if(boss_fight_shoot_cooldown > 0) boss_fight_shoot_cooldown--;
    if(THIS->custom_data[CD_INVINCIBILITY] > 0) THIS->custom_data[CD_INVINCIBILITY]--;

    if(THIS->custom_data[CD_INVINCIBILITY] > 0 && (THIS->custom_data[CD_INVINCIBILITY] / 6) % 2 == 0) {
        SPRITE_SET_DMG_PALETTE(THIS, 1);
    } else {
        SPRITE_SET_DMG_PALETTE(THIS, 0);
    }

    if(KEY_PRESSED(J_UP)) {
        if (!BossFightTranslateSprite(THIS, 0, -1)) moved = 1;
        boss_fight_player_direction = DIR_UP;
    }
    if(KEY_PRESSED(J_DOWN)) {
        if (!BossFightTranslateSprite(THIS, 0, 1)) moved = 1;
        boss_fight_player_direction = DIR_DOWN;
    }
    if(KEY_PRESSED(J_LEFT)) {
        if (!BossFightTranslateSprite(THIS, -1, 0)) moved = 1;
        boss_fight_player_direction = DIR_LEFT;
    }
    if(KEY_PRESSED(J_RIGHT)) {
        if (!BossFightTranslateSprite(THIS, 1, 0)) moved = 1;
        boss_fight_player_direction = DIR_RIGHT;
    }

    if (moved) {
        THIS->custom_data[CD_WALK_TIMER]++;
        SetPlayerWalkFrame();
    } else {
        SetPlayerIdleFrame();
    }

    if(KEY_PRESSED(J_B) && boss_fight_shoot_cooldown == 0) {
        Sprite* projectile = SafeSpriteAddEx(SpriteScrew, THIS->x, THIS->y, (UINT8)boss_fight_player_direction);
        /* NULL = pool full: skip the shot, retry next frame. */
        if (projectile) {
            projectile->custom_data[CD_DIR] = (UINT8)boss_fight_player_direction;
            boss_fight_shoot_cooldown = SHOOT_COOLDOWN;
            PlayScrewShotSound();
        }
    }
}

void DESTROY() {
    boss_fight_player = NULL;
}
