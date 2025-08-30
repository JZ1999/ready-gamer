#include <stdio.h>
#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "Print.h"

Direction player_direction;

extern UINT16 ready_coins; // Player's currency

#define SHOOT_COOLDOWN       100  // Adjust for how many frames between shots
#define PLAYER_MAX_HEALTH    1
#define CD_PLAYER_HEALTH     0
#define CD_INVINCIBILITY     1
#define INVINCIBILITY_FRAMES 60

UINT8 shoot_cooldown;

// Helper function to check for closed door collision
UINT8 CollidesWithClosedDoor(Sprite* player, INT16 dx, INT16 dy) {
    UINT8 i;
    Sprite* spr;
    SPRITEMANAGER_ITERATE(i, spr) {
        if (IsDoorType(spr->type) && spr->custom_data[CD_DOOR_STATE] == 0) {
            // Simulate next position
            INT16 orig_x = player->x;
            INT16 orig_y = player->y;
            player->x += dx;
            player->y += dy;
            UINT8 collides = CheckCollision(player, spr);
            player->x = orig_x;
            player->y = orig_y;
            if (collides) return 1;
        }
    }
    return 0;
}

/**
 * Check if player is adjacent to a door (within +1 tile) for buying
 * @param player Pointer to the player sprite
 * @return Pointer to adjacent door sprite, or NULL if none found
 */
Sprite* GetAdjacentDoorForPurchase(Sprite* player) {
    UINT8 i;
    Sprite* spr;
    SPRITEMANAGER_ITERATE(i, spr) {
        if (IsDoorType(spr->type) && spr->custom_data[CD_DOOR_STATE] == 0) {
            // Check if door is adjacent (within 1 tile in any direction)
            INT16 dx = (INT16)player->x - (INT16)spr->x;
            INT16 dy = (INT16)player->y - (INT16)spr->y;
            DPRINT_POS(6, 0);
            
            // Check if within 1 tile distance (8 pixels per tile)
            if (dx >= -17 && dx <= 17 && dy >= -17 && dy <= 17) {
                return spr;
            }
        }
    }
    return NULL;
}

void TakeDamage(Sprite* player) {
    if(player->custom_data[CD_INVINCIBILITY] > 0) return;

    if(player->custom_data[CD_PLAYER_HEALTH] > 0) {
        player->custom_data[CD_PLAYER_HEALTH]--;
        player->custom_data[CD_INVINCIBILITY] = INVINCIBILITY_FRAMES;

        if(player->custom_data[CD_PLAYER_HEALTH] == 0) {
            UINT8 i;
            Sprite* spr;
            SPRITEMANAGER_ITERATE(i, spr) {
                if (spr->unique_id == THIS->unique_id) {
                    SpriteManagerRemove(i);
                    break;
                }
            }
        }
    }
}

/**
 * Handle door interaction logic
 * @param door_sprite Pointer to the door sprite
 * @return 1 if door interaction was handled, 0 otherwise
 */
UINT8 HandleDoorInteraction(Sprite* door_sprite) {
    // Check if door is closed
    if (door_sprite->custom_data[CD_DOOR_STATE] != 0) {
        return 0; // Door is already open
    }
    
    UINT8 door_cost = door_sprite->custom_data[CD_DOOR_COST];
    
    // Check if player has enough coins
    if (ready_coins >= door_cost) {
        ready_coins -= door_cost;
        door_sprite->custom_data[CD_DOOR_STATE] = 1; // Open the door
        // Find and remove the door sprite
        UINT8 i;
        Sprite* spr;
        SPRITEMANAGER_ITERATE(i, spr) {
            if (spr == door_sprite) {
                SpriteManagerRemove(i);
                break;
            }
        }
        DPRINT_POS(0, 0);
        Printf("     Door opened!     ");
        return 1; // Door interaction handled
    } else {
        DPRINT_POS(0, 0);
        Printf("Need %d Ready Coins!\n", door_cost);
        return 1; // Door interaction handled (insufficient coins)
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

    // === Collision Detection and Handling ===
    UINT8 i;
    Sprite* spr;
    UINT8 door_interaction_handled = 0;
    
    // Check for adjacent door for purchase (not collision-based)
    Sprite* adjacent_door = GetAdjacentDoorForPurchase(THIS);
    if (adjacent_door && !door_interaction_handled) {
        door_interaction_handled = HandleDoorInteraction(adjacent_door);
    }
    
    // Handle direct collisions with enemies
    SPRITEMANAGER_ITERATE(i, spr) {
        if (CheckCollision(THIS, spr)) {
            // Handle enemy collisions
            if (spr->type == BasicVirus || spr->type == SpeedVirus) {
                TakeDamage(THIS);
                break; // Only damage once per frame
            }
        }
    }

    if(KEY_PRESSED(J_UP)) {
        if (!CollidesWithClosedDoor(THIS, 0, -1)) {
            TranslateSprite(THIS, 0, -1);
        }
        SetFrame(THIS, 1);
        player_direction = DIR_UP;
    } 
    if(KEY_PRESSED(J_DOWN)) {
        if (!CollidesWithClosedDoor(THIS, 0, 1)) {
            TranslateSprite(THIS, 0, 1);
        }
        SetFrame(THIS, 2);
        player_direction = DIR_DOWN;
    }
    if(KEY_PRESSED(J_LEFT)) {
        if (!CollidesWithClosedDoor(THIS, -1, 0)) {
            TranslateSprite(THIS, -1, 0);
        }
        THIS->mirror = NO_MIRROR;
        SetFrame(THIS, 0);
        player_direction = DIR_LEFT;
    }
    if(KEY_PRESSED(J_RIGHT)) {
        if (!CollidesWithClosedDoor(THIS, 1, 0)) {
            TranslateSprite(THIS, 1, 0);
        }
        THIS->mirror = V_MIRROR;
        SetFrame(THIS, 0);
        player_direction = DIR_RIGHT;
    }

	if(KEY_PRESSED(J_B) && shoot_cooldown == 0) {
        Sprite* screw = SpriteManagerAddEx(SpriteScrew, THIS->x, THIS->y, (UINT8)player_direction);
        screw->custom_data[CD_DIR] = (UINT8)player_direction;
        shoot_cooldown = SHOOT_COOLDOWN;
    }

}

void DESTROY() {
}