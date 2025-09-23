#include <stddef.h>
#include <rand.h>

#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "Scroll.h"
#include "SpriteManager.h"
#include "Music.h"
#include "Print.h"
#include "SpriteData.h"
#include "SoundEffects.h"

#define RANDOM rand()
#define ENEMY_SPAWN_DELAY 180   // frames between spawns
#define NEXT_ROUND_TIMER 300

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define EDGE_PADDING 9
#define SCREEN_TILE_WIDTH 20
#define MAX_Y_ENEMY_SPAWN 118

#define CENTER_X(text_len) ((SCREEN_TILE_WIDTH - text_len) / 2)

// Max enemies per level
#define MAX_ENEMIES_PER_LEVEL 10
#define MAX_LEVELS 6


IMPORT_TILES(font);

DECLARE_MUSIC(track1);

IMPORT_MAP(map);

extern UINT8 current_level;

UINT8 enemies_killed = 0; // Global counter for killed enemies
UINT8 enemies_to_spawn;
UINT8 spawn_timer = 0;         // timer for delay
UINT8 enemies_left_to_spawn = 0; // how many still to spawn

UINT8 next_round_timer = NEXT_ROUND_TIMER;   // frames between levels
UINT8 current_level = 1;
UINT8 waiting_for_start = 1;

UINT16 ready_coins = 0; // Player's currency

const UINT8 level_spawns[MAX_LEVELS][MAX_ENEMIES_PER_LEVEL] = {
    {ENEMY_TYPE_SPEED, ENEMY_TYPE_BASIC},
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC},
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_TANK, ENEMY_TYPE_SPEED},
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC, ENEMY_TYPE_SPEED, ENEMY_TYPE_SPEED},
    {ENEMY_TYPE_TANK, ENEMY_TYPE_TANK, ENEMY_TYPE_BASIC, ENEMY_TYPE_SPEED},
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_SPEED, ENEMY_TYPE_SPEED, ENEMY_TYPE_BASIC}
};

// How many enemies each level has
const UINT8 level_lengths[MAX_LEVELS] = {2, 3, 3, 4, 4, 4};
UINT8 enemy_spawn_index = 0;

// Tile indices for different brick types
#define TILE_EMPTY 0
#define TILE_FULL_BRICK 1
#define TILE_PARTIAL_BRICK_1 2
#define TILE_PARTIAL_BRICK_2 3

// Spawn point positions (fixed positions for the two spawn points)
#define SPAWN_POINT_1_X 20
#define SPAWN_POINT_1_Y 20
#define SPAWN_POINT_2_X 100
#define SPAWN_POINT_2_Y 80

// Get a random spawn point position
void GetRandomSpawnPosition(UINT8* x, UINT8* y) {
    // Choose randomly between the two spawn points
    if (RANDOM % 2 == 0) {
        *x = SPAWN_POINT_1_X;
        *y = SPAWN_POINT_1_Y;
    } else {
        *x = SPAWN_POINT_2_X;
        *y = SPAWN_POINT_2_Y;
    }
}

// Wall avoidance movement function for enemies
UINT8 EnemyMoveWithWallAvoidance(Sprite* enemy, INT16 dx, INT16 dy) {
    // Try the original movement first using ZGB's built-in collision
    UINT8 result = TranslateSprite(enemy, dx, dy);
    
    // If TranslateSprite returned a tile collision (non-zero), try avoidance
    if (result != 0) {
        // If blocked, try to move up or down based on player position
        INT16 avoid_dy = 0;
        
        if (scroll_target->y < enemy->y) {
            // Player is above, try to move up
            avoid_dy = -1;
        } else if (scroll_target->y > enemy->y) {
            // Player is below, try to move down
            avoid_dy = 1;
        } else {
            // Player is at same Y level, try up first, then down
            avoid_dy = -1;
        }
        
        // Try the avoidance movement
        result = TranslateSprite(enemy, 0, avoid_dy);
        
        // If still blocked, try the opposite direction
        if (result != 0) {
            if (avoid_dy == -1) {
                avoid_dy = 1;
            } else {
                avoid_dy = -1;
            }
            
            result = TranslateSprite(enemy, 0, avoid_dy);
        }
    }
    
    return result;
}

// Custom collision detection for partial brick tiles
UINT8 CheckPartialBrickCollision(Sprite* sprite, INT16 dx, INT16 dy) {
    // Get the tile at the sprite's position
    UINT8 tile_x = (sprite->x + dx) >> 3;
    UINT8 tile_y = (sprite->y + dy) >> 3;
    
    // Get the tile type at this position
    UINT8 tile = GetScrollTile(tile_x, tile_y);
    
    // Check if it's a partial brick tile
    if (tile == TILE_PARTIAL_BRICK_1 || tile == TILE_PARTIAL_BRICK_2) {
        // For partial bricks, use quarter-based collision
        UINT8 sprite_bottom_y = sprite->y + sprite->coll_h - 1 + dy;
        UINT8 tile_top_y = tile_y * 8;
        UINT8 tile_quarter_y = tile_top_y + 2; // Quarter from top (2 pixels)
        UINT8 tile_three_quarter_y = tile_top_y + 6; // Three quarters from top (6 pixels)
        
        if (tile == TILE_PARTIAL_BRICK_1) {
            // Partial brick 1: collision in top quarter
            if (sprite_bottom_y <= tile_quarter_y) {
                return 1;
            }
        } else if (tile == TILE_PARTIAL_BRICK_2) {
            // Partial brick 2: collision in bottom quarter
            if (sprite_bottom_y >= tile_three_quarter_y) {
                return 1;
            }
        }
    }
    
    return 0;
}

// Enhanced collision check that includes partial brick logic
UINT8 CheckCustomTileCollision(Sprite* sprite, INT16 dx, INT16 dy) {
    // First check the standard collision (full brick tiles)
    UINT8 tile_x = (sprite->x + dx) >> 3;
    UINT8 tile_y = (sprite->y + dy) >> 3;
    
    // Check bounds
    if (tile_x >= scroll_tiles_w || tile_y >= scroll_tiles_h) {
        return 0;
    }
    
    // Get the tile type
    UINT8 tile = GetScrollTile(tile_x, tile_y);
    
    // Check if it's a full brick tile (index 1)
    if (tile == TILE_FULL_BRICK) {
        return 1; // Collision detected
    }
    
    // Check for partial brick collision
    if (CheckPartialBrickCollision(sprite, dx, dy)) {
        return 1;
    }
    
    // Return 0 for empty space and other non-collision tiles
    return 0;
}

void SpawnEnemies() {
     if (enemies_left_to_spawn > 0 && enemy_spawn_index < enemies_to_spawn) {
        if (--spawn_timer == 0) {
            UINT8 x, y;
            GetRandomSpawnPosition(&x, &y);

            UINT8 type = level_spawns[current_level - 1][enemy_spawn_index]; // current_level is 1-based

            Sprite* virus = NULL;

            switch(type) {
                case ENEMY_TYPE_BASIC:
                    virus = SpriteManagerAdd(BasicVirus, x, y);
                    break;
                case ENEMY_TYPE_SPEED:
                    virus = SpriteManagerAdd(SpeedVirus, x, y);
                    break;
                case ENEMY_TYPE_TANK:
                    virus = SpriteManagerAdd(TankVirus, x, y);
                    break;
            }

            if (virus) {
                if (type == ENEMY_TYPE_TANK) {
                    virus->custom_data[CD_ENEMY_HEALTH] = 5;
                } else {
                    virus->custom_data[CD_ENEMY_HEALTH] = 3;
                }
            }

            enemies_left_to_spawn--;
            enemy_spawn_index++;
            spawn_timer = ENEMY_SPAWN_DELAY; // reset timer
        }
    }
}


void CheckForNextLevel() {
    // If all enemies are killed and no more left to spawn, proceed to next level after a delay
    if (enemies_left_to_spawn <= 0 && enemies_killed == enemies_to_spawn) {
        if(--next_round_timer > 0) return;

        next_round_timer = NEXT_ROUND_TIMER; // reset timer

        current_level++;
        
        // Update level display and reset enemy spawning for next level
        DPRINT_POS(0, 0);
        DPrintf("       Level %d      ", current_level);
        
        enemies_to_spawn = level_lengths[current_level - 1];
        enemies_left_to_spawn = enemies_to_spawn;
        enemies_killed = 0;
        spawn_timer = ENEMY_SPAWN_DELAY;
        enemy_spawn_index = 0;
        
        // Update door cost for next level
        UINT8 i;
        Sprite* spr;
        // Ensure spawn points are still present for next level
        UINT8 spawn_point_count = 0;
        SPRITEMANAGER_ITERATE(i, spr) {
            if (spr->type == SpawnPoint) {
                spawn_point_count++;
            }
        }
        
        // If spawn points are missing, recreate them
        if (spawn_point_count < 2) {
            SpriteManagerAdd(SpawnPoint, SPAWN_POINT_1_X, SPAWN_POINT_1_Y);
            SpriteManagerAdd(SpawnPoint, SPAWN_POINT_2_X, SPAWN_POINT_2_Y);
        }
    }
}


void LoadLevel(UINT8 level) {
    if (level >= MAX_LEVELS) level = MAX_LEVELS - 1;

    // Clear all existing sprites first
    SpriteManagerReset();
    
    // Spawn a new player
    scroll_target = SpriteManagerAdd(SpritePlayer, 90, 50);
    
    // Spawn a Door to the right of the player
    Sprite* door = SpriteManagerAdd(Door, 115, 52); // x=120, y=50
    if (door) {
        door->custom_data[CD_DOOR_STATE] = 0; // Closed
        door->custom_data[CD_DOOR_COST] = 10; // Cost in Ready Coins (custom property)
    }
    
    // Spawn the two spawn point sprites
    SpriteManagerAdd(SpawnPoint, SPAWN_POINT_1_X, SPAWN_POINT_1_Y);
    SpriteManagerAdd(SpawnPoint, SPAWN_POINT_2_X, SPAWN_POINT_2_Y);

    enemies_to_spawn = level_lengths[level - 1];
    enemies_left_to_spawn = enemies_to_spawn;
    spawn_timer = ENEMY_SPAWN_DELAY;
    enemy_spawn_index = 0;

    DPRINT_POS(0, 0);
    DPrintf("       Level %d      ", current_level);
}

void START() {
    // Only full brick tile (index 1) has collision
    // Empty space (index 0) and partial bricks (index 2, 3) will be handled with custom collision
    UINT8 collision_tiles[] = { 1, 0 };
    InitScroll(BANK(map), &map, collision_tiles, 0);

    INIT_CONSOLE(font, 2); // Increase console height to 2 lines
    DPRINT_POS(0, 0);
    DPrintf("       Level %d      ", current_level);
    DPRINT_POS(0, 1);
    DPrintf("Ready Coins: %d", ready_coins);
    PlayMusic(track1, LOOP);
    
    // Spawn the two spawn point sprites
    SpriteManagerAdd(SpawnPoint, SPAWN_POINT_1_X, SPAWN_POINT_1_Y);
    SpriteManagerAdd(SpawnPoint, SPAWN_POINT_2_X, SPAWN_POINT_2_Y);
    
    // Load the initial level (this will spawn the player and door)
    LoadLevel(current_level);
}

void CheckForPlayerDeath() {
    // Check if player sprite still exists
    UINT8 i;
    Sprite* spr;
    UINT8 player_exists = 0;
    
    SPRITEMANAGER_ITERATE(i, spr) {
        if (spr->type == SpritePlayer) {
            player_exists = 1;
            break;
        }
    }
    
    // If player doesn't exist, restart the level
    if (!player_exists) {
        // Reset level state
        waiting_for_start = 1;
        enemies_to_spawn = 0;
        enemies_left_to_spawn = 0;
        enemies_killed = 0;
        spawn_timer = ENEMY_SPAWN_DELAY;
        enemy_spawn_index = 0;
        current_level = 1;
        ready_coins = 0;
        
        // Clear the screen and show restart message
        DPRINT_POS(0, 0);
        DPrintf("   GAME OVER!   ");
        DPRINT_POS(0, 1);
        DPrintf("  Press any key");
    }
}

void UPDATE() {
    if(waiting_for_start) {
        if(joypad()) {
            // Clear text
            initarand(DIV_REG);
            LoadLevel(current_level);
            waiting_for_start = 0;
        }
        return;
    }

    // Check if player is still alive
    CheckForPlayerDeath();
    
    // If waiting for restart, don't continue with game logic
    if(waiting_for_start) {
        return;
    }

    DPRINT_POS(0, 1);
    DPrintf("Ready Coins: %d       ", ready_coins);

    SpawnEnemies();
    
    CheckForNextLevel();
    UpdateDoorOpeningMelody();
    UpdateEnemyHitMelody();
}