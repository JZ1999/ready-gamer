#include "Banks/SetAutoBank.h"
#include "main.h"

#include <gb/gb.h>
#include "ZGBMain.h"
#include "Scroll.h"
#include "SpriteManager.h"
#include "BossFight.h"
#include "SoundEffects.h"

/*
 * Static single-screen arena after the boss-run corridor (StateBossRun hands
 * off here once the corridor ends — see that file). The map is exactly
 * SCREENWIDTH x SCREENHEIGHT (see BossFight.h), so there is nothing to
 * scroll: no CameraDriver, scroll_target stays NULL, scroll_x/y never move.
 *
 * Win: this file just counts live Boss sprites every frame; once that hits
 * zero (main boss, or both phase-2 split copies, dead) -> StateWin. Boss.c
 * owns its own HP/state machine and the phase-2 split entirely; this state
 * doesn't need to know which boss is which.
 */

extern UINT8 last_tile_loaded;
extern UINT8 last_bg_pal_loaded;
extern INT8 scroll_h_border;
extern Sprite* boss_slots[2];

#define BOSS_MAIN_HP 20

void START() {
    HIDE_WIN;
    SetWindowY(144);
    scroll_h_border = 0;

    last_tile_loaded = 0;
    last_bg_pal_loaded = 0;
    scroll_offset_x = 0;
    scroll_offset_y = 0;
    scroll_target = NULL;

    SpriteManagerReset();
    InitBossFightScroll();

    boss_slots[0] = NULL;
    boss_slots[1] = NULL;

    SpriteManagerAdd(BossFightPlayer, 24, 60);
    SpriteManagerAddEx(Boss, 112, 50, BOSS_MAIN_HP);

    PlayBossFightMusicStart(); // Background music for the whole fight

    SHOW_BKG;
    SHOW_SPRITES;
}

void UPDATE() {
    UINT8 i;
    Sprite* spr;
    UINT8 boss_count = 0;

    PlayBossFightMusicUpdate();

    SPRITEMANAGER_ITERATE(i, spr) {
        if (spr->type == Boss) {
            boss_count++;
        }
    }

    if (boss_count == 0) {
        SetState(StateWin);
    }
}

void DESTROY() {
}
