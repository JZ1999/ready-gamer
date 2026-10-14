#include "Banks/SetAutoBank.h"

#include "SpriteManager.h"
#include "Scroll.h"
#include "ZGBMain.h"
#include "SpriteData.h"
#include "SpriteBudget.h"
#include "SoundEffects.h"
#include "StateGame.h"

/*
 * Rapid-fire shot (fire-rate chip). A small plasma dart from the screw sheet
 * (frame 12 vertical, 13 horizontal; mirrored for up/left). Moves 2 px per
 * frame (two 1 px steps so it can't skip through a wall or an enemy), does
 * normal damage, and lives ~1 s (about the same range as a screw).
 */

#define RAPID_FRAME_V 12
#define RAPID_FRAME_H 13
#define RAPID_LIFETIME 60

extern UINT8 enemies_killed;
extern UINT16 ready_coins;

static void KillVirus(Sprite* virus, UINT8 virus_id) {
    if (virus && virus->type == BomberVirus) {
        SafeSpriteAdd(Bomb, virus->x, virus->y); /* NULL if pool full: bomb just skipped */
    }
    ++enemies_killed;
    ready_coins += KillCoinValue(virus->type); /* 1 coin, 2 for Tank/Charge */
    PlayCoinCollectSound();
    SpriteManagerRemove(virus_id);
}

/* Returns 1 if the dart hit something and was removed. */
static UINT8 CheckHits(void) {
    UINT8 i;
    Sprite* spr;
    SPRITEMANAGER_ITERATE(i, spr) {
        if (IsEnemyType(spr->type) && CheckCollision(THIS, spr)) {
            UINT8 hit_type = spr->type;
            if (spr->custom_data[CD_ENEMY_HEALTH] > PROJECTILE_DAMAGE_NORMAL) {
                spr->custom_data[CD_ENEMY_HEALTH] -= PROJECTILE_DAMAGE_NORMAL;
                spr->custom_data[CD_BLINK_TIMER] = 10;
            } else {
                KillVirus(spr, i);
            }
            PlayEnemyHitSoundFor(hit_type);
            SpriteManagerRemove(THIS_IDX);
            return 1;
        }
        if (spr->type == Bomb && CheckCollision(THIS, spr)) {
            SpriteManagerRemove(i);        /* defuse bomb */
            SpriteManagerRemove(THIS_IDX);
            return 1;
        }
    }
    return 0;
}

void START() {
    switch (THIS->custom_data[CD_DIR]) {
        case DIR_UP:    SetFrame(THIS, RAPID_FRAME_V); THIS->mirror = H_MIRROR; break;
        case DIR_DOWN:  SetFrame(THIS, RAPID_FRAME_V); THIS->mirror = NO_MIRROR; break;
        case DIR_LEFT:  SetFrame(THIS, RAPID_FRAME_H); THIS->mirror = V_MIRROR; break;
        default:        SetFrame(THIS, RAPID_FRAME_H); THIS->mirror = NO_MIRROR; break;
    }
    THIS->custom_data[CD_LIFETIME] = RAPID_LIFETIME;
}

void UPDATE() {
    UINT8 step;
    INT8 dx = 0;
    INT8 dy = 0;
    switch (THIS->custom_data[CD_DIR]) {
        case DIR_UP:    dy = -1; break;
        case DIR_DOWN:  dy = 1; break;
        case DIR_LEFT:  dx = -1; break;
        default:        dx = 1; break;
    }
    for (step = 0; step != 2; ++step) {
        if (SafeTranslateSprite(THIS, dx, dy)) {
            SpriteManagerRemove(THIS_IDX); /* hit a wall */
            return;
        }
        if (CheckHits()) return;
    }
    if (--THIS->custom_data[CD_LIFETIME] == 0) {
        SpriteManagerRemove(THIS_IDX);
    }
}

void DESTROY() {
}
