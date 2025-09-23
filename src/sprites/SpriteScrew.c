#include "Banks/SetAutoBank.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "ZGBMain.h"
#include "SpriteData.h"
#include "SoundEffects.h"

#define MAX_SPRITES 20

#define FRAME_DELAY 10
#define TOTAL_FRAMES 3

#define CD_ANIM_TIMER    1
#define CD_ANIM_OFFSET   2
#define CD_LIFETIME      3

#define SCREW_LIFETIME   120  // Frames before self-destruct (about 2 seconds at 60fps)

extern UINT8 enemies_killed;
extern UINT16 ready_coins;

void KillVirus(UINT8 virus_id) {
    ++enemies_killed;
    ++ready_coins; // Each enemy gives 1 Ready Coin
    PlayCoinCollectSound(); // Play coin collection sound
    SpriteManagerRemove(virus_id);
}


void START() {
    if(THIS->custom_data[CD_DIR] == 2 || THIS->custom_data[CD_DIR] == 3) { // Left or Right
        THIS->custom_data[CD_ANIM_OFFSET] = 3; // Frame offset
    } else {
        THIS->custom_data[CD_ANIM_OFFSET] = 0;
    }

    SetFrame(THIS, THIS->custom_data[2]);
    THIS->custom_data[CD_ANIM_TIMER] = FRAME_DELAY;
    THIS->custom_data[CD_LIFETIME] = SCREW_LIFETIME; // Initialize lifetime

    // Flipping based on direction
    switch(THIS->custom_data[0]) {
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
    // Movement based on direction
    switch(THIS->custom_data[CD_DIR]) {
        case 0: TranslateSprite(THIS, 0, -1); break; // Up
        case 1: TranslateSprite(THIS, 0, 1); break;  // Down
        case 2: TranslateSprite(THIS, -1, 0); break; // Left
        case 3: TranslateSprite(THIS, 1, 0); break;  // Right
    }

    // === ENEMY COLLISION ===
    UINT8 i;
    Sprite* spr;
    SPRITEMANAGER_ITERATE(i, spr) {
		if (IsEnemyType(spr->type)) {
			if (CheckCollision(THIS, spr)) {
                if(spr->custom_data[CD_ENEMY_HEALTH] > 1) {
                    spr->custom_data[CD_ENEMY_HEALTH]--;
                    spr->custom_data[CD_BLINK_TIMER] = 10; // Set hit feedback timer (higher value for more visible effect)
                    PlayEnemyHitSound(); // Play hit sound
                } else {
                    UINT8 virus_id = i;
                    KillVirus(virus_id);
                    PlayEnemyHitSound(); // Play hit sound
                }
                SpriteManagerRemove(THIS_IDX); // Remove bullet
                return;
			}
		}
	}

    // Frame animation with correct offset
    if(--THIS->custom_data[CD_ANIM_TIMER] == 0) {
        THIS->custom_data[CD_ANIM_TIMER] = FRAME_DELAY;
        UINT8 relative_frame = (THIS->anim_frame - THIS->custom_data[CD_ANIM_OFFSET] + 1) % TOTAL_FRAMES;
        SetFrame(THIS, THIS->custom_data[CD_ANIM_OFFSET] + relative_frame);
    }

    // Self-destruct timer
    if(--THIS->custom_data[CD_LIFETIME] == 0) {
        SpriteManagerRemove(THIS_IDX);
    }
}

void DESTROY() {
    THIS->custom_data[CD_DIR] = 0;
    THIS->custom_data[CD_ANIM_TIMER] = 0;
    THIS->custom_data[CD_ANIM_OFFSET] = 0;
    THIS->custom_data[CD_LIFETIME] = 0;
}