#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "Scroll.h"
#include "SpriteManager.h"
#include "Music.h"
#include "Print.h"
#include "SpriteData.h"
#include <rand.h>

#define RANDOM rand()
#define ENEMY_SPAWN_DELAY 180   // frames between spawns
#define NEXT_ROUND_TIMER 300

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144

#define EDGE_PADDING 9
#define SCREEN_TILE_WIDTH 20
#define MAX_Y_ENEMY_SPAWN 118

#define CENTER_X(text_len) ((SCREEN_TILE_WIDTH - text_len) / 2)

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


// Returns a random X/Y position along the border
void GetRandomEdgePosition(UINT8* x, UINT8* y) {
    UINT8 side = RANDOM % 4; // 0: top, 1: bottom, 2: left, 3: right
    switch(side) {
        case 0: // Top
            *x = (RANDOM % (SCREEN_WIDTH - EDGE_PADDING * 2)) + EDGE_PADDING;
            *y = EDGE_PADDING;
            break;
        case 1: // Bottom
            *x = (RANDOM % (SCREEN_WIDTH - EDGE_PADDING * 2)) + EDGE_PADDING;
            *y = MAX_Y_ENEMY_SPAWN;
            break;
        case 2: // Left
            *x = EDGE_PADDING;
            *y = (RANDOM % (SCREEN_HEIGHT - EDGE_PADDING * 2)) + EDGE_PADDING;
            break;
        case 3: // Right
            *x = SCREEN_WIDTH - EDGE_PADDING;
            *y = (RANDOM % (SCREEN_HEIGHT - EDGE_PADDING * 2)) + EDGE_PADDING;
            break;
    }
}

void SpawnEnemies() {
     if (enemies_left_to_spawn > 0) {
        if (--spawn_timer == 0) {
            UINT8 x, y;
            GetRandomEdgePosition(&x, &y);
            Sprite* virus = SpriteManagerAdd(BasicVirus, x, y);
            if (virus) {
                virus->custom_data[CD_ENEMY_HEALTH] = 3;
            }

            enemies_left_to_spawn--;
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
        waiting_for_start = 1;
        enemies_to_spawn = 0;
        enemies_killed = 0;
        spawn_timer = ENEMY_SPAWN_DELAY;
    }
}


void LoadLevel(UINT8 level) {
    enemies_to_spawn = 1 + level;

    enemies_left_to_spawn = enemies_to_spawn;
    spawn_timer = ENEMY_SPAWN_DELAY;

    DPRINT_POS(6, 0);
    DPrintf("Level %d", current_level);
}

void START() {
    scroll_target = SpriteManagerAdd(SpritePlayer, 90, 50);

    UINT8 collision_tiles[] = { 1, 0 };
    InitScroll(BANK(map), &map, collision_tiles, 0);

    INIT_CONSOLE(font, 1);
    DPRINT_POS(6, 0);
    DPrintf("Level %d", current_level);
    PlayMusic(track1, LOOP);
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

    SpawnEnemies();
    
    CheckForNextLevel();
}