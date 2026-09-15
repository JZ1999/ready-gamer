/*
 * Pinned to bank 6 on purpose (same bank as mapbossarena's own data, which
 * only uses ~370 of its 16384 bytes — plenty of room). NOT auto-banked
 * (#pragma bank 255): the auto-bank tool put this file in bank 3 — the
 * SAME bank StateBossFight.c itself landed in — and every BANKED call from
 * StateBossFight.c into this file, whatever it actually did internally
 * (map reload, tile reload, both, neither — tested each in isolation),
 * blanked the screen or crashed (BGB: invalid opcode, PC inside OAM) the
 * instant the call happened. Calling a BANKED function whose target bank
 * happens to equal the bank already selected reproduced every time; a
 * lenient JS emulator never caught it, only BGB (real hardware timing)
 * did. Forcing a genuinely different target bank turns this into the same
 * ordinary cross-bank call pattern used everywhere else in this codebase
 * (e.g. Rooms.c's *FromTable helpers) instead of a same-bank edge case.
 */
#pragma bank 6

#include "ZGBMain.h"
#include "BossFight.h"
#include "Math.h"
#include "BankManager.h"
#include "Scroll.h"
#include "main.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "SoundEffects.h"
#include <gb/gb.h>
#include <string.h>

/*
 * BossFightTakeDamage (player-damage) and BossBeginSwordAttack (boss's
 * melee punish) also live here, not in BossFightPlayer.c/Boss.c where
 * they're conceptually "about" — same same-bank-BANKED-call hazard as
 * above: Boss.c, BossFightPlayer.c, BossBulletAimed.c and BossSwordHitbox.c
 * all auto-banked into bank 3 together, and each of these two functions is
 * called from at least one *other* bank-3 file. Bank 6 (this file) isn't
 * called into by any of them for anything else, so every real call site
 * ends up a genuine cross-bank call.
 */
extern UINT8 player_lives;

#define PLAYER_MAX_HEALTH    1
#define INVINCIBILITY_FRAMES 60
#define RESPAWN_INVINCIBILITY_FRAMES 180

/*
 * StateBossFight support — own small collision helper for the same reason as
 * ZGBMain.c's BossRunTileBlocked/BossRunTranslateSprite: mapbossarena only
 * ever uses tiles 0/1 (floor/wall), independent of the room-collision code
 * and of BossRun's own (different map, different size).
 *
 * The tile grid (only 360 bytes: 20x18) is copied into WRAM once, in
 * InitBossFightScroll, instead of PUSH_BANK-ing into mapbossarena's ROM
 * bank on every single tile check — a cheap win once collision checks run
 * every frame, several sprites x several times each.
 */

IMPORT_MAP(mapbossarena);
extern const unsigned char mapbossarena_map[];

#define TILE_FULL_BRICK 1

static const UINT8 scroll_collision_tiles[] = { 1, 0 };
static UINT8 tile_cache[BOSSFIGHT_MAP_TILES_W * BOSSFIGHT_MAP_TILES_H];

void InitBossFightScroll(void) BANKED {
	UINT16 i;

	InitScroll(BANK(mapbossarena), &mapbossarena, scroll_collision_tiles, 0);

	PUSH_BANK(BANK(mapbossarena));
	for (i = 0; i != sizeof(tile_cache); i++) {
		tile_cache[i] = mapbossarena_map[i];
	}
	POP_BANK;
}

static UINT8 BossFightTileBlocked(UINT16 x, UINT16 y) {
	if (x >= BOSSFIGHT_MAP_TILES_W || y >= BOSSFIGHT_MAP_TILES_H) {
		return 1; /* out of bounds counts as blocked, keeps sprites on the map */
	}

	return tile_cache[y * BOSSFIGHT_MAP_TILES_W + x] == TILE_FULL_BRICK;
}

static UINT8 BossFightCheckCollision(UINT16 px, UINT16 py, UINT8 coll_w, UINT8 coll_h, INT8 dx, INT8 dy) {
	INT16 nx = (INT16)px + dx;
	INT16 ny = (INT16)py + dy;
	UINT8 left_tile, right_tile, top_tile, bottom_tile;

	if (U_LESS_THAN(nx, 0) || (UINT16)(nx + coll_w - 1) >= BOSSFIGHT_MAP_PIXELS_W ||
	    U_LESS_THAN(ny, 0) || (UINT16)(ny + coll_h - 1) >= BOSSFIGHT_MAP_PIXELS_H) {
		return 1;
	}

	left_tile = (UINT8)(nx >> 3);
	right_tile = (UINT8)((nx + coll_w - 1) >> 3);
	top_tile = (UINT8)(ny >> 3);
	bottom_tile = (UINT8)((ny + coll_h - 1) >> 3);

	if (BossFightTileBlocked(left_tile, top_tile)) return 1;
	if (BossFightTileBlocked(right_tile, top_tile)) return 1;
	if (BossFightTileBlocked(left_tile, bottom_tile)) return 1;
	if (BossFightTileBlocked(right_tile, bottom_tile)) return 1;

	return 0;
}

UINT8 BossFightTranslateSprite(Sprite* sprite, INT8 dx, INT8 dy) BANKED {
	UINT16 px = sprite->x;
	UINT16 py = sprite->y;

	if ((dx || dy) && BossFightCheckCollision(px, py, sprite->coll_w, sprite->coll_h, dx, dy)) {
		return 1;
	}

	if (dx) sprite->x = (UINT16)((INT16)px + dx);
	if (dy) sprite->y = (UINT16)((INT16)py + dy);

	return 0;
}

void BossFightTakeDamage(Sprite* player) BANKED {
	if (player->custom_data[CD_INVINCIBILITY] > 0) return;

	if (player->custom_data[CD_PLAYER_HEALTH] > 0) {
		player->custom_data[CD_PLAYER_HEALTH]--;
		PlayBossArenaDamageSound();

		if (player->custom_data[CD_PLAYER_HEALTH] == 0) {
			if (player_lives > 0) {
				player_lives--;
				player->custom_data[CD_PLAYER_HEALTH] = PLAYER_MAX_HEALTH;
				player->custom_data[CD_INVINCIBILITY] = RESPAWN_INVINCIBILITY_FRAMES;
			} else {
				UINT8 i;
				Sprite* spr;
				SPRITEMANAGER_ITERATE(i, spr) {
					if (spr->unique_id == player->unique_id) {
						SpriteManagerRemove(i);
						SetState(StateGameOver);
						break;
					}
				}
			}
		} else {
			player->custom_data[CD_INVINCIBILITY] = INVINCIBILITY_FRAMES;
		}
	}
}

/* Called by BossBulletAimed on a real near-miss: swap the boss to the
 * bullet's position and start the telegraph -> sword-swing sequence. */
void BossBeginSwordAttack(Sprite* boss, UINT16 x, UINT16 y) BANKED {
	boss->x = x;
	boss->y = y;
	boss->custom_data[CD_BOSS_STATE] = STATE_SWORD_TELEGRAPH;
	boss->custom_data[CD_BOSS_TIMER] = SWORD_TELEGRAPH_FRAMES;
}
