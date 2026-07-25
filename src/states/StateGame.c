#include <stddef.h>
#include <rand.h>

#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "Math.h"
#include "Scroll.h"
#include "SpriteManager.h"
#include "Music.h"
#include "Print.h"
#include "SpriteData.h"
#include "SoundEffects.h"
#include "BankManager.h"
#include "Rooms.h"
#include "StateGame.h"
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
IMPORT_MAP(map);

DECLARE_MUSIC(track1);

static const UINT8 collision_tiles[] = { 1, 0 };

extern UINT8 last_tile_loaded;
extern UINT8 last_bg_pal_loaded;

extern UINT8 current_level;
extern UINT8 player_electric_attack;

UINT8 enemies_killed = 0; // Global counter for killed enemies
UINT8 enemies_to_spawn;
UINT8 spawn_timer = 0;         // timer for delay
UINT8 enemies_left_to_spawn = 0; // how many still to spawn

UINT8 next_round_timer = NEXT_ROUND_TIMER;   // frames between levels
UINT8 current_level = 1;
UINT8 waiting_for_start = 1;
UINT8 pending_room_transition = 0;
UINT8 pending_electric_pickup = 0;

UINT16 ready_coins = 0; // Player's currency

const UINT8 level_spawns[MAX_LEVELS][MAX_ENEMIES_PER_LEVEL] = {
    /* Level 1: basics only */
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC},
    /* Level 2+: BomberVirus introduced */
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_SPEED, ENEMY_TYPE_BOMBER},
    /* Level 3+: ChargeVirus introduced */
    {ENEMY_TYPE_CHARGE, ENEMY_TYPE_BASIC, ENEMY_TYPE_BOMBER},
    {ENEMY_TYPE_BOMBER, ENEMY_TYPE_CHARGE, ENEMY_TYPE_SPEED, ENEMY_TYPE_BASIC, ENEMY_TYPE_TANK},
    {ENEMY_TYPE_TANK, ENEMY_TYPE_BOMBER, ENEMY_TYPE_CHARGE, ENEMY_TYPE_SPEED},
    {ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED, ENEMY_TYPE_CHARGE, ENEMY_TYPE_TANK}
};

// How many enemies each level has
const UINT8 level_lengths[MAX_LEVELS] = {2, 3, 3, 5, 4, 4};
UINT8 enemy_spawn_index = 0;

void SyncGameHud(void) {
    INIT_CONSOLE(font, 2);
    DPRINT_POS(0, 1);
    DPrintf("Ready Coins: %d       ", ready_coins);
}

void StartRoomEnemyWave(void) {
    UINT8 wave_index = current_room;

    if (wave_index >= MAX_LEVELS) {
        wave_index = MAX_LEVELS - 1;
    }

    current_level = wave_index + 1;
    enemies_to_spawn = level_lengths[wave_index];
    enemies_left_to_spawn = enemies_to_spawn;
    enemies_killed = 0;
    spawn_timer = ENEMY_SPAWN_DELAY;
    enemy_spawn_index = 0;
}

void SpawnEnemies() {
     if (enemies_left_to_spawn > 0 && enemy_spawn_index < enemies_to_spawn) {
        if (--spawn_timer == 0) {
            UINT8 x, y;
            GetRandomSpawnPositionFromTable(&x, &y);

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
                case ENEMY_TYPE_BOMBER:
                    virus = SpriteManagerAdd(BomberVirus, x, y);
                    break;
                case ENEMY_TYPE_CHARGE:
                    virus = SpriteManagerAdd(ChargeVirus, x, y);
                    break;
            }

            if (virus) {
                // Base HP +1 per level above 1 (room waves bump current_level)
                UINT8 base_health = (type == ENEMY_TYPE_TANK) ? 5 : 3;
                UINT8 level_bonus = (current_level > 1) ? (current_level - 1) : 0;
                virus->custom_data[CD_ENEMY_HEALTH] = base_health + level_bonus;
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
        
        EnsureRoomSpawnPointsFromTable();
    }
}


void LoadLevel(UINT8 level) {
    if (level >= MAX_LEVELS) level = MAX_LEVELS - 1;

    SpawnRoomFromTable(0);
    current_level = level;
    StartRoomEnemyWave();

    DPRINT_POS(0, 0);
    DPrintf("       Level %d      ", current_level);
}

void START() {
    scroll_target = NULL;
    last_tile_loaded = 0;
    last_bg_pal_loaded = 0;
    scroll_offset_x = 0;
    scroll_offset_y = 0;
    current_room = 0;

    InitRoomScrollFromTable(0);

    INIT_CONSOLE(font, 2);
    DPRINT_POS(0, 0);
    DPrintf("       Level %d      ", current_level);
    DPRINT_POS(0, 1);
    DPrintf("Ready Coins: %d       ", ready_coins);

    initarand(DIV_REG);
    PlayMusic(track1, LOOP);
    waiting_for_start = 0;
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
        current_room = 0;
        ready_coins = 0;
        player_electric_attack = 0;
        
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

    if (pending_room_transition) {
        UINT8 next_room = current_room + 1;

        pending_room_transition = 0;

        if (next_room >= room_count) {
            return;
        }

        DISPLAY_OFF;
        wait_vbl_done();

        LoadRoomFromTable(next_room);

        wait_vbl_done();
        scroll_x_vblank = scroll_x;
        scroll_y_vblank = scroll_y;
        SCX_REG = scroll_x_vblank + (scroll_offset_x << 3);
        SCY_REG = scroll_y_vblank + (scroll_offset_y << 3);
        DISPLAY_ON;

        SyncGameHud();
        StartRoomEnemyWave();
        DPRINT_POS(0, 0);
        DPrintf("       Level %d      ", current_level);
        return;
    }

    if (pending_electric_pickup) {
        pending_electric_pickup = 0;
        player_electric_attack = 1;
        if (scroll_target) {
            scroll_target->custom_data[CD_PLAYER_ELECTRIC] = 1;
        }
        PlayCoinCollectSound();
    }

    DPRINT_POS(0, 1);
    DPrintf("Ready Coins: %d       ", ready_coins);

    SpawnEnemies();
    
    CheckForNextLevel();
    UpdateDoorOpeningMelody();
    UpdateEnemyHitMelody();
}