#include <stdio.h>
#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "Print.h"

Direction player_direction;

#define SHOOT_COOLDOWN       100  // Adjust for how many frames between shots
#define PLAYER_MAX_HEALTH    1
#define CD_PLAYER_HEALTH     0
#define CD_INVINCIBILITY     1
#define INVINCIBILITY_FRAMES 60

UINT8 shoot_cooldown;

void TakeDamage(Sprite* player) {
    if(player->custom_data[CD_INVINCIBILITY] > 0) return;

    if(player->custom_data[CD_PLAYER_HEALTH] > 0) {
        player->custom_data[CD_PLAYER_HEALTH]--;
        player->custom_data[CD_INVINCIBILITY] = INVINCIBILITY_FRAMES;

        if(player->custom_data[CD_PLAYER_HEALTH] == 0) {
            UINT8 i;
            Sprite* spr;
            SPRITEMANAGER_ITERATE(i, spr) {
                if (spr->unique_id != THIS->unique_id) {
                    continue;
                }
                SpriteManagerRemove(i);
            }
        }
    }
}

void START() {
    SetFrame(THIS, 0);
    player_direction = DIR_LEFT;
	shoot_cooldown = 0;

    THIS->custom_data[CD_PLAYER_HEALTH] = PLAYER_MAX_HEALTH;
    THIS->custom_data[CD_INVINCIBILITY] = 0;
}

void UPDATE() {
    if(shoot_cooldown > 0) shoot_cooldown--;
    if(THIS->custom_data[CD_INVINCIBILITY] > 0) THIS->custom_data[CD_INVINCIBILITY]--;

    if(KEY_PRESSED(J_UP)) {
        TranslateSprite(THIS, 0, -1);
        SetFrame(THIS, 1);
        player_direction = DIR_UP;
    } 
    if(KEY_PRESSED(J_DOWN)) {
        TranslateSprite(THIS, 0, 1);
        SetFrame(THIS, 2);
        player_direction = DIR_DOWN;
    }
    if(KEY_PRESSED(J_LEFT)) {
        TranslateSprite(THIS, -1, 0);
        THIS->mirror = NO_MIRROR;
        SetFrame(THIS, 0);
        player_direction = DIR_LEFT;
    }
    if(KEY_PRESSED(J_RIGHT)) {
        TranslateSprite(THIS, 1, 0);
        THIS->mirror = V_MIRROR;
        SetFrame(THIS, 0);
        player_direction = DIR_RIGHT;
    }

	if(KEY_PRESSED(J_B) && shoot_cooldown == 0) {
        Sprite* screw = SpriteManagerAddEx(SpriteScrew, THIS->x, THIS->y, (UINT8)player_direction);
        screw->custom_data[CD_DIR] = (UINT8)player_direction;
        shoot_cooldown = SHOOT_COOLDOWN;
    }

    // === Enemy Collision Check ===
    UINT8 i;
    Sprite* spr;
    SPRITEMANAGER_ITERATE(i, spr) {
        if(
            spr->type == BasicVirus || spr->type == SpeedVirus
        ) {
            if(CheckCollision(THIS, spr)) {
                TakeDamage(THIS);
                break; // Only damage once per frame
            }
        }
    }
}

void DESTROY() {
}