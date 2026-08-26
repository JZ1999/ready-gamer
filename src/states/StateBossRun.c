#include "Banks/SetAutoBank.h"
#include "main.h"

#include <gb/gb.h>
#include "ZGBMain.h"
#include "Scroll.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "BossRun.h"

/*
 * Auto-scroll "dodge the boss" mode — separate from the room-exploration
 * game entirely (no RoomDef/doors/spawns involved). See SESSION_NOTES.md
 * for the design reasoning (why a new state, why no boss body sprite, why
 * mapboss.gbm is generated rather than hand-painted).
 *
 * Camera: driven automatically by CameraDriver (see that file) being
 * `scroll_target` — the engine's own per-frame camera-follow does the
 * rest, nothing here calls MoveScroll directly.
 * Win: scroll clamps at the map's right edge once the camera can't scroll
 * further — checked below instead of a separate distance/timer.
 * Lose: BossRunPlayer/BossBullet reuse the normal game's lives/respawn
 * system (BossRunTakeDamage, a copy of SpritePlayer's TakeDamage) — it
 * calls SetState(StateGameOver) itself once lives run out, nothing to
 * check here.
 */

#define BULLET_SPAWN_INTERVAL 90 // ~1.5s at 60fps — tune to taste

extern UINT8 last_tile_loaded;
extern UINT8 last_bg_pal_loaded;
extern INT8 scroll_h_border;

extern Sprite* boss_run_player;

static UINT16 bullet_timer;

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
    InitBossRunScroll();

    bullet_timer = BULLET_SPAWN_INTERVAL;

    scroll_target = SpriteManagerAdd(CameraDriver, 0, 0);
    SpriteManagerAdd(BossRunPlayer, 16, 72);

    SHOW_BKG;
    SHOW_SPRITES;
}

void UPDATE() {
    /* CameraDriver sits at y=200 (kept off the visible 144px map so its
     * sprite graphic is never seen), but the engine's auto-follow camera
     * (RefreshScroll, called from SpriteManagerUpdate right before this)
     * also centers scroll_y on scroll_target->y — so it was dragging the
     * vertical scroll down and clipping the map's top wall row out of
     * view. Force it back every frame; the map's height exactly matches
     * the screen height, so scroll_y should always be 0 here anyway. */
    scroll_y = 0;

    if (scroll_x >= (INT16)(BOSSRUN_MAP_PIXELS_W - SCREENWIDTH)) {
        SetState(StateWin);
        return;
    }

    if (--bullet_timer == 0) {
        bullet_timer = BULLET_SPAWN_INTERVAL;
        if (boss_run_player) {
            SpriteManagerAdd(BossBullet, scroll_x + SCREENWIDTH - 8, boss_run_player->y);
        }
    }
}

void DESTROY() {
}
