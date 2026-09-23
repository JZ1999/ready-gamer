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
#include "SoftReset.h"
#include "SpriteBudget.h"
#define RANDOM rand()
#define ENEMY_SPAWN_DELAY 180   // frames between spawns
#define NEXT_ROUND_TIMER 300
#define SPAWN_RETRY_DELAY 30    // frames to wait before retrying a spawn when the sprite pool is full

// DEBUG: start directly in this room index for testing — set back to 0 for normal start
#define DEBUG_START_ROOM 0
// DEBUG: start at this wave level (1-20) — set back to 1 for normal start
#define DEBUG_START_LEVEL 1
// DEBUG: start with the electric weapon already unlocked — set back to 0 for normal start
#define DEBUG_START_ELECTRIC 0
// DEBUG: start with this many Ready Coins — set back to 0 for normal start
#define DEBUG_START_COINS 0
// DEBUG: initial player_lives override for testing (boss run/fight without
// dying constantly) — set to STARTING_LIVES for normal boot. Separate from
// STARTING_LIVES itself so the real balance value isn't touched.
#define DEBUG_STARTING_LIVES STARTING_LIVES

#define STARTING_LIVES 3

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define EDGE_PADDING 9
#define SCREEN_TILE_WIDTH 20
#define MAX_Y_ENEMY_SPAWN 118

#define CENTER_X(text_len) ((SCREEN_TILE_WIDTH - text_len) / 2)

// Max enemies per level
#define MAX_ENEMIES_PER_LEVEL 10
#define MAX_LEVELS 20


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

UINT16 next_round_timer = NEXT_ROUND_TIMER;  // frames between levels (UINT16: 300 does not fit in a UINT8)
UINT8 current_level = 1;
UINT8 waiting_for_start = 1;
UINT8 pending_room_transition = 0;
UINT8 pending_electric_pickup = 0;

UINT16 ready_coins = 0; // Player's currency
UINT8 player_lives = DEBUG_STARTING_LIVES;

/*
 * 20 wave tables — difficulty ramps by count + enemy mix.
 * After level 20, CheckForNextLevel wraps back to 1.
 */
const UINT8 level_spawns[MAX_LEVELS][MAX_ENEMIES_PER_LEVEL] = {
    /* 1–3: tutorial / basics */
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC},
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC},
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_SPEED, ENEMY_TYPE_BASIC},
    /* 4–5: Bomber enters */
    {ENEMY_TYPE_BASIC, ENEMY_TYPE_SPEED, ENEMY_TYPE_BOMBER},
    {ENEMY_TYPE_SPEED, ENEMY_TYPE_BOMBER, ENEMY_TYPE_BASIC, ENEMY_TYPE_BASIC},
    /* 6–7: Charge enters */
    {ENEMY_TYPE_CHARGE, ENEMY_TYPE_BASIC, ENEMY_TYPE_BOMBER},
    {ENEMY_TYPE_CHARGE, ENEMY_TYPE_SPEED, ENEMY_TYPE_BOMBER, ENEMY_TYPE_BASIC},
    /* 8–9: Tank enters */
    {ENEMY_TYPE_TANK, ENEMY_TYPE_BASIC, ENEMY_TYPE_SPEED, ENEMY_TYPE_BOMBER},
    {ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED},
    /* 10–12: full roster, growing packs */
    {ENEMY_TYPE_BOMBER, ENEMY_TYPE_CHARGE, ENEMY_TYPE_TANK, ENEMY_TYPE_SPEED, ENEMY_TYPE_BASIC},
    {ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED, ENEMY_TYPE_BOMBER},
    {ENEMY_TYPE_CHARGE, ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED},
    /* 13–15: heavier elites */
    {ENEMY_TYPE_TANK, ENEMY_TYPE_BOMBER, ENEMY_TYPE_CHARGE, ENEMY_TYPE_SPEED, ENEMY_TYPE_TANK},
    {ENEMY_TYPE_CHARGE, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_BOMBER, ENEMY_TYPE_TANK, ENEMY_TYPE_SPEED},
    {ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_TANK, ENEMY_TYPE_SPEED, ENEMY_TYPE_CHARGE},
    /* 16–18: dense pressure */
    {ENEMY_TYPE_BOMBER, ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_SPEED, ENEMY_TYPE_BOMBER, ENEMY_TYPE_TANK},
    {ENEMY_TYPE_TANK, ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED},
    {ENEMY_TYPE_CHARGE, ENEMY_TYPE_TANK, ENEMY_TYPE_BOMBER, ENEMY_TYPE_CHARGE, ENEMY_TYPE_TANK, ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED},
    /* 19–20: climax (then loops to 1) */
    {ENEMY_TYPE_TANK, ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED, ENEMY_TYPE_BASIC},
    {ENEMY_TYPE_TANK, ENEMY_TYPE_TANK, ENEMY_TYPE_CHARGE, ENEMY_TYPE_CHARGE, ENEMY_TYPE_BOMBER, ENEMY_TYPE_BOMBER, ENEMY_TYPE_BOMBER, ENEMY_TYPE_SPEED, ENEMY_TYPE_SPEED}
};

const UINT8 level_lengths[MAX_LEVELS] = {
    2, 3, 3, 3, 4,
    3, 4, 4, 4, 5,
    5, 5, 5, 6, 6,
    6, 7, 7, 8, 9
};
UINT8 enemy_spawn_index = 0;

void SyncGameHud(void) {
    INIT_CONSOLE(font, 2);
    DPRINT_POS(0, 1);
    DPrintf("Coins:%d Lives:%d    ", ready_coins, player_lives);
}

static void ClampLevel(void) {
    if (current_level < 1) {
        current_level = 1;
    }
    while (current_level > MAX_LEVELS) {
        current_level -= MAX_LEVELS;
    }
}

void StartRoomEnemyWave(void) {
    UINT8 wave_index;

    ClampLevel();
    wave_index = current_level - 1;
    enemies_to_spawn = level_lengths[wave_index];
    enemies_left_to_spawn = enemies_to_spawn;
    enemies_killed = 0;
    spawn_timer = ENEMY_SPAWN_DELAY;
    enemy_spawn_index = 0;
}

void SpawnEnemies() {
     if (enemies_left_to_spawn > 0 && enemy_spawn_index < enemies_to_spawn) {
        if (--spawn_timer == 0) {
            UINT16 x, y;
            GetRandomSpawnPositionFromTable(&x, &y);

            UINT8 type = level_spawns[current_level - 1][enemy_spawn_index]; // current_level is 1-based

            Sprite* virus = NULL;

            /* Sprite pool nearly full (see SpriteBudget.h): don't consume this
               spawn slot, retry shortly. Without this the wave would count an
               enemy that never existed and never clear. */
            if (!POOL_HAS_ROOM_LOW()) {
                spawn_timer = SPAWN_RETRY_DELAY;
                return;
            }

            switch(type) {
                case ENEMY_TYPE_BASIC:
                    virus = SafeSpriteAddLow(BasicVirus, x, y);
                    break;
                case ENEMY_TYPE_SPEED:
                    virus = SafeSpriteAddLow(SpeedVirus, x, y);
                    break;
                case ENEMY_TYPE_TANK:
                    virus = SafeSpriteAddLow(TankVirus, x, y);
                    break;
                case ENEMY_TYPE_BOMBER:
                    virus = SafeSpriteAddLow(BomberVirus, x, y);
                    break;
                case ENEMY_TYPE_CHARGE:
                    virus = SafeSpriteAddLow(ChargeVirus, x, y);
                    break;
            }

            if (virus) {
                /* Gentle HP ramp across 20 looping levels (cap +6). */
                UINT8 base_health = (type == ENEMY_TYPE_TANK) ? 5 : 3;
                UINT8 level_bonus = (current_level > 1) ? ((current_level - 1) / 3) : 0;
                if (level_bonus > 6) {
                    level_bonus = 6;
                }
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
        if (current_level > MAX_LEVELS) {
            current_level = 1; /* loop waves forever until rooms are cleared */
        }

        DPRINT_POS(0, 0);
        DPrintf("       Level %d      ", current_level);

        StartRoomEnemyWave();
        EnsureRoomSpawnPointsFromTable();
    }
}


void LoadLevel(UINT8 level) {
    SpawnRoomFromTable(current_room);
    current_level = level;
    ClampLevel();
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
    current_room = DEBUG_START_ROOM;
    current_level = DEBUG_START_LEVEL;
    ready_coins = DEBUG_START_COINS;
    player_lives = DEBUG_STARTING_LIVES;
    player_electric_attack = DEBUG_START_ELECTRIC;
    pending_room_transition = 0;
    pending_electric_pickup = 0;

    /* Wipe the full 32x32 VRAM background tilemap before drawing the room.
     * The room's own scroll streams in tiles only near the camera, so any
     * leftover text a previous state wrote straight to background tiles
     * (StateGameOver/StateWin's "PRESS A", via PRINT_BKG) survives in the
     * wrapped-around area outside what's been scrolled over yet, and can
     * resurface as stray glyphs mid-room once the camera later reaches that
     * wrapped position. Display is already off here (see main()'s loop). */
    fill_bkg_rect(0, 0, 32, 32, 0);

    InitRoomScrollFromTable(current_room);

    INIT_CONSOLE(font, 2);
    DPRINT_POS(0, 0);
    DPrintf("       Level %d      ", current_level);
    DPRINT_POS(0, 1);
    DPrintf("Coins:%d Lives:%d    ", ready_coins, player_lives);

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
        current_room = DEBUG_START_ROOM;
        ready_coins = DEBUG_START_COINS;
        player_electric_attack = 0;

        // Clear the screen and show restart message
        DPRINT_POS(0, 0);
        DPrintf("   GAME OVER!   ");
        DPRINT_POS(0, 1);
        DPrintf("  Press any key");
    }
}

void UPDATE() {
    CHECK_SOFT_RESET();
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

        /* MAX_ROOMS (compile-time constant), not room_count: room_count is a
         * ROM const that lives in Rooms.c's own bank, and reading it directly
         * from here (StateGame.c's bank) is a cross-bank read with no bank
         * switch — it can silently read whatever byte happens to be at that
         * address in THIS bank instead. That let next_room=5 slip past this
         * check into LoadRoomFromTable(5), which sets current_room=5 with no
         * bounds check of its own (see ZGBMain.c) — an invalid room that
         * later made CheckForPlayerDeath's sprite scan misfire (GAME OVER
         * printed while still in-room, root cause of the "crash entering
         * autoscroll" report). */
        if (next_room >= MAX_ROOMS) {
            /* Cleared final room portal → boss run; it calls SetState(StateWin)
             * itself once the corridor's far end is reached. */
            SetState(StateBossRun);
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
        /* Keep wave progression across rooms; bump one tier on room entry. */
        current_level++;
        if (current_level > MAX_LEVELS) {
            current_level = 1;
        }
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
    DPrintf("Coins:%d Lives:%d    ", ready_coins, player_lives);

    SpawnEnemies();
    
    CheckForNextLevel();
    UpdateDoorOpeningMelody();
    UpdateEnemyHitMelody();
}