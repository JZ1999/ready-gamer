#include <stdio.h>
#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "Print.h"
#include "SoundEffects.h"
#include "Scroll.h"
#include "Math.h"
#include "StateGame.h"

Direction player_direction;

extern UINT16 ready_coins;
UINT8 player_electric_attack = 0;

#define SHOOT_COOLDOWN       100
#define PLAYER_MAX_HEALTH    1
#define INVINCIBILITY_FRAMES 60
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

UINT8 shoot_cooldown;

static void SetPlayerIdleFrame(void) {
    switch (player_direction) {
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

    switch (player_direction) {
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

// Custom movement using GetScrollTile (safe across ROM banks)
static UINT8 CustomTranslateSprite(Sprite* sprite, INT8 x, INT8 y) {
    return SafeTranslateSprite(sprite, x, y);
}

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
                    SetState(StateGameOver);
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
        PlayDoorOpenSound(); // Play door opening sound
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
    THIS->custom_data[CD_WALK_TIMER] = 0;
    THIS->custom_data[CD_PLAYER_ELECTRIC] = player_electric_attack;
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
            if (IsEnemyType(spr->type)) {
                TakeDamage(THIS);
                break; // Only damage once per frame
            }
        }
    }

    UINT8 moved = 0;

    if(KEY_PRESSED(J_UP)) {
        if (!CollidesWithClosedDoor(THIS, 0, -1) && !CustomTranslateSprite(THIS, 0, -1)) {
            moved = 1;
        }
        player_direction = DIR_UP;
    } 
    if(KEY_PRESSED(J_DOWN)) {
        if (!CollidesWithClosedDoor(THIS, 0, 1) && !CustomTranslateSprite(THIS, 0, 1)) {
            moved = 1;
        }
        player_direction = DIR_DOWN;
    }
    if(KEY_PRESSED(J_LEFT)) {
        if (!CollidesWithClosedDoor(THIS, -1, 0) && !CustomTranslateSprite(THIS, -1, 0)) {
            moved = 1;
        }
        player_direction = DIR_LEFT;
    }
    if(KEY_PRESSED(J_RIGHT)) {
        if (!CollidesWithClosedDoor(THIS, 1, 0) && !CustomTranslateSprite(THIS, 1, 0)) {
            moved = 1;
        }
        player_direction = DIR_RIGHT;
    }

    if (moved) {
        THIS->custom_data[CD_WALK_TIMER]++;
        SetPlayerWalkFrame();
    } else {
        SetPlayerIdleFrame();
    }

	if(KEY_PRESSED(J_B) && shoot_cooldown == 0) {
        UINT8 projectile_type = THIS->custom_data[CD_PLAYER_ELECTRIC] ? ElectricProjectile : SpriteScrew;
        Sprite* projectile = SpriteManagerAddEx(projectile_type, THIS->x, THIS->y, (UINT8)player_direction);

        projectile->custom_data[CD_DIR] = (UINT8)player_direction;
        shoot_cooldown = SHOOT_COOLDOWN;
        PlayScrewShotSound();
    }
}

void DESTROY() {
}