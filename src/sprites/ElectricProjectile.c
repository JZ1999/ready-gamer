#include "Banks/SetAutoBank.h"

#include "SpriteManager.h"
#include "Scroll.h"
#include "ZGBMain.h"
#include "SpriteData.h"
#include "SoundEffects.h"
#include "StateGame.h"

#define FRAME_DELAY 10
#define TOTAL_FRAMES 3
#define ELECTRIC_FRAME_FIRST 6
#define PROJECTILE_LIFETIME 120

extern UINT8 enemies_killed;
extern UINT16 ready_coins;

static void KillVirus(Sprite* virus, UINT8 virus_id) {
    if (virus && virus->type == BomberVirus) {
        SpriteManagerAdd(Bomb, virus->x, virus->y);
    }

    ++enemies_killed;
    ++ready_coins;
    PlayCoinCollectSound();
    SpriteManagerRemove(virus_id);
}

static void DamageEnemy(Sprite* enemy, UINT8 virus_id) {
    UINT8 damage = PROJECTILE_DAMAGE_ELECTRIC;

    if (enemy->custom_data[CD_ENEMY_HEALTH] > damage) {
        enemy->custom_data[CD_ENEMY_HEALTH] -= damage;
        enemy->custom_data[CD_BLINK_TIMER] = 10;
        PlayEnemyHitSound();
    } else {
        KillVirus(enemy, virus_id);
        PlayEnemyHitSound();
    }
}

void START() {
    SetFrame(THIS, ELECTRIC_FRAME_FIRST);
    THIS->custom_data[CD_ANIM_TIMER] = FRAME_DELAY;
    THIS->custom_data[CD_LIFETIME] = PROJECTILE_LIFETIME;

    switch (THIS->custom_data[CD_DIR]) {
        case DIR_UP:
            THIS->mirror = H_MIRROR;
            break;
        case DIR_DOWN:
            THIS->mirror = NO_MIRROR;
            break;
        case DIR_LEFT:
            THIS->mirror = V_MIRROR;
            break;
        case DIR_RIGHT:
            THIS->mirror = NO_MIRROR;
            break;
    }
}

void UPDATE() {
    switch (THIS->custom_data[CD_DIR]) {
        case DIR_UP: SafeTranslateSprite(THIS, 0, -1); break;
        case DIR_DOWN: SafeTranslateSprite(THIS, 0, 1); break;
        case DIR_LEFT: SafeTranslateSprite(THIS, -1, 0); break;
        case DIR_RIGHT: SafeTranslateSprite(THIS, 1, 0); break;
    }

    {
        UINT8 i;
        Sprite* spr;
        SPRITEMANAGER_ITERATE(i, spr) {
            if (IsEnemyType(spr->type)) {
                if (CheckCollision(THIS, spr)) {
                    DamageEnemy(spr, i);
                    SpriteManagerRemove(THIS_IDX);
                    return;
                }
            }
            if (spr->type == Bomb) {
                if (CheckCollision(THIS, spr)) {
                    SpriteManagerRemove(i);
                    SpriteManagerRemove(THIS_IDX);
                    return;
                }
            }
        }
    }

    if (--THIS->custom_data[CD_ANIM_TIMER] == 0) {
        UINT8 relative_frame;

        THIS->custom_data[CD_ANIM_TIMER] = FRAME_DELAY;
        relative_frame = (THIS->anim_frame - ELECTRIC_FRAME_FIRST + 1) % TOTAL_FRAMES;
        SetFrame(THIS, ELECTRIC_FRAME_FIRST + relative_frame);
    }

    if (--THIS->custom_data[CD_LIFETIME] == 0) {
        SpriteManagerRemove(THIS_IDX);
    }
}

void DESTROY() {
    THIS->custom_data[CD_DIR] = 0;
    THIS->custom_data[CD_ANIM_TIMER] = 0;
    THIS->custom_data[CD_LIFETIME] = 0;
}
