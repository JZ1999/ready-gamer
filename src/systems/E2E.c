#include "Banks/SetAutoBank.h"

#include "E2E.h"
#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "StateGame.h"
#include "Rooms.h"
#include "Scroll.h"
#include "Print.h"
#include "FlowField.h"
#include "main.h"

#include <gb/gb.h>
#include <gbdk/emu_debug.h>
#include <string.h>

#if E2E_USES_PLANNER
#include "E2EPlan.h" /* whole-map planner: E2EPlan.c, its own ROM bank */
#endif

/*
 * Honest in-ROM e2e autopilot.
 *
 * The bot only decides which BUTTONS to hold. Each frame it writes
 * `e2e_keys`; the player sprites OR it into ZGB's `keys` at the top of their
 * UPDATE (E2E_INJECT_KEYS), so every game rule applies exactly as for a human:
 * 1 px/frame movement, wall + closed-door collision, SHOOT_COOLDOWN, facing
 * direction for shots, automatic door purchase, pickups, damage, lives.
 *
 * The bot never writes game state: no sprite moves, no projectile spawns, no
 * coin / door / room / power-up changes. It only READS state (sprite
 * positions, coins, shoot cooldown, current room) to pick its inputs.
 *
 * Verdict: on PASS the final frame is painted all-lightest, on FAIL
 * all-darkest (palette registers only), then EMU_BREAKPOINT. The runner reads
 * that frame, so a breakpoint alone is never a pass.
 */

#if E2E_ENABLED

UINT8 e2e_keys; /* consumed by E2E_INJECT_KEYS() */
/* Telemetry for the headless runner (read from RAM by symbol, never acted on). */
UINT16 e2e_px;
UINT16 e2e_py;
UINT8 e2e_dbg;   /* which bot branch ran this frame */
UINT8 e2e_dbg2;
UINT16 e2e_updates; /* game-loop updates (vs emulated frames = lag) */
UINT8 e2e_t_rapid;    /* fire-rate chip owned (telemetry) */
extern UINT8 player_rapid_fire;
UINT8 e2e_et;  /* nearest enemy type (telemetry) */
UINT8 e2e_skip_portal; /* a power-up is still in this room: don't take the portal yet */
UINT16 e2e_ex;
UINT8 e2e_t_cost;
INT8 e2e_t_block;
UINT8 e2e_t_errand;
UINT8 e2e_t_plan;
UINT8 e2e_t_doors;
UINT16 e2e_ey;

extern UINT16 ready_coins;
extern UINT8 player_electric_attack;
extern UINT8 current_room;
extern Sprite* scroll_target;
extern Sprite* boss_run_player;
extern Sprite* boss_fight_player;
extern UINT8 shoot_cooldown;            /* SpritePlayer.c   (read only) */
extern UINT8 boss_run_shoot_cooldown;   /* BossRunPlayer.c  (read only) */
extern UINT8 boss_fight_shoot_cooldown; /* BossFightPlayer.c(read only) */
extern UINT8 enemies_killed;            /* StateGame.c      (read only) */
extern UINT8 enemies_to_spawn;          /* StateGame.c      (read only) */
extern UINT8 enemies_left_to_spawn;     /* StateGame.c      (read only) */

#define DIR_KEYS (J_UP | J_DOWN | J_LEFT | J_RIGHT)

#define E2E_BOOT_FRAMES     60
#define E2E_ACTION_DELAY    30
#define E2E_VERDICT_HOLD    60  /* frames the result stays visible before the verdict frame */
#define E2E_THREAT_RANGE    40  /* manhattan px: closer than this, fight/kite first */

#if E2E_SCENARIO == E2E_SCENARIO_DIE
#define E2E_TIMEOUT_FRAMES (60UL * 120UL)
#elif E2E_SCENARIO == E2E_SCENARIO_COINS
#define E2E_TIMEOUT_FRAMES (60UL * 300UL)
#elif E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR
/* Whole game incl. farming: allow 2 game hours (Linux runner: a few minutes).
 * Far too long for BGB -Capture (2 GB AVI cap) - use the Linux runner. */
#define E2E_TIMEOUT_FRAMES (60UL * 7200UL)
#else
/* Keep under ~14 min: BGB's -Capture AVI hits its 2 GB cap around 15 min. */
#define E2E_TIMEOUT_FRAMES (60UL * 840UL)
#endif

#define E2E_PHASE_BOOT  0
#define E2E_PHASE_PLAY  1
#define E2E_PHASE_RUN   2
#define E2E_PHASE_DONE  3

#define E2E_VERDICT_NONE 0
#define E2E_VERDICT_PASS 1
#define E2E_VERDICT_FAIL 2

static UINT32 e2e_frames;
static UINT8 e2e_phase;
static UINT8 e2e_wait;
static UINT8 e2e_verdict;
static UINT8 e2e_hold;
static UINT8 e2e_done;
static UINT16 e2e_coins_before;
static UINT8 e2e_max_room; /* StateGame resets current_room on the death frame */
static UINT8 e2e_errand;   /* electric: coins banked, heading out */

/* Stuck detection / detour (wall-follow) state. */
static UINT16 e2e_last_x;
static UINT16 e2e_last_y;
static UINT8 e2e_stuck;
static UINT8 e2e_detour_keys;
static UINT8 e2e_detour_frames;
static UINT8 e2e_detour_flip;
static UINT8 e2e_detour_len;

static INT16 Abs16(INT16 v) { return v < 0 ? (INT16)(-v) : v; }

/* ---------------------------------------------------------------- verdict */

static void E2E_Finish(UINT8 verdict) {
	if (e2e_verdict) return;
	e2e_verdict = verdict;
	e2e_hold = E2E_VERDICT_HOLD;
	e2e_keys = 0;
	if (verdict == E2E_VERDICT_PASS) {
		EMU_MESSAGE("E2E PASS");
	} else {
		EMU_MESSAGE("E2E FAIL");
	}
}

/* Returns 1 while a verdict is pending/done (bot must stop driving). */
static UINT8 E2E_TickVerdict(void) {
	if (!e2e_verdict) {
		if (e2e_frames < 0xFFFFFFFFUL) e2e_frames++;
		if (e2e_frames >= E2E_TIMEOUT_FRAMES) {
			EMU_MESSAGE("E2E FAIL timeout");
			E2E_Finish(E2E_VERDICT_FAIL);
		}
		return e2e_verdict != E2E_VERDICT_NONE;
	}
	e2e_keys = 0;
	if (e2e_done) return 1;
	if (e2e_hold) {
		e2e_hold--;
		if (e2e_hold == 4) {
			UINT8 pal = (e2e_verdict == E2E_VERDICT_PASS) ? 0x00 : 0xFF;
			BGP_REG = pal;
			OBP0_REG = pal;
			OBP1_REG = pal;
		}
		return 1;
	}
	e2e_done = 1;
	EMU_BREAKPOINT;
	return 1;
}

/* ------------------------------------------------------------ queries */

static Sprite* E2E_FindType(UINT8 type) {
	UINT8 i;
	Sprite* spr;
	SPRITEMANAGER_ITERATE(i, spr) {
		if (spr->type == type) return spr;
	}
	return 0;
}

static UINT16 E2E_Dist(Sprite* a, UINT16 x, UINT16 y) {
	return (UINT16)Abs16((INT16)x - (INT16)a->x) + (UINT16)Abs16((INT16)y - (INT16)a->y);
}

/* Hazards (enemies, boss, bombs, spawn points), collected once per bot
 * update: the distance queries below used to walk the whole sprite pool
 * several times a frame. */
#define E2E_HZ_MAX 16
static Sprite* e2e_hz[E2E_HZ_MAX];
static UINT16 e2e_hz_path[E2E_HZ_MAX]; /* enemies: walking distance (px) from the player, 0 = unknown */
static UINT8 e2e_nhz;
static UINT16 e2e_hz_px;  /* player centre when cached */
static UINT16 e2e_hz_py;

static void E2E_CacheHazards(void) {
	UINT8 i;
	Sprite* spr;
	e2e_nhz = 0;
	if (scroll_target) {
		e2e_hz_px = (UINT16)(scroll_target->x + (scroll_target->coll_w >> 1));
		e2e_hz_py = (UINT16)(scroll_target->y + (scroll_target->coll_h >> 1));
	}
	SPRITEMANAGER_ITERATE(i, spr) {
		if (!IsEnemyType(spr->type) && spr->type != Boss && spr->type != Bomb && spr->type != SpawnPoint) continue;
		if (e2e_nhz == E2E_HZ_MAX) break;
		e2e_hz_path[e2e_nhz] = 0;
		if (IsEnemyType(spr->type) && current_state == StateGame) {
			/* The game's own flow field (what enemies follow): an enemy behind
			 * a wall is as far away as its walk, not its straight line. */
			UINT8 t = FlowFieldDistTo(spr);
			if (t < 254) e2e_hz_path[e2e_nhz] = (UINT16)t << 3;
		}
		e2e_hz[e2e_nhz++] = spr;
	}
}

static Sprite* E2E_NearestEnemy(Sprite* from, UINT16* out_d) {
	UINT8 i;
	Sprite* spr;
	Sprite* best = 0;
	UINT16 best_d = 0xFFFF; /* was UINT8: enemies > 255 px away (big maze maps) were invisible */
	if (from) {
		for (i = 0; i != e2e_nhz; ++i) {
			UINT16 d;
			spr = e2e_hz[i];
			if (!IsEnemyType(spr->type) && spr->type != Boss) continue;
			d = E2E_Dist(from, spr->x, spr->y);
			if (d < best_d) {
				best_d = d;
				best = spr;
			}
		}
	}
	if (out_d) *out_d = best_d;
	return best;
}

/* Nearest thing that can hurt us: enemies, the boss, bombs. */
static Sprite* E2E_NearestHazard(Sprite* from, UINT16* out_d) {
	UINT8 i;
	Sprite* spr;
	Sprite* best = 0;
	UINT16 best_d = 0xFFFF;
	if (from) {
		for (i = 0; i != e2e_nhz; ++i) {
			UINT16 d;
			spr = e2e_hz[i];
			d = E2E_Dist(from, spr->x, spr->y);
			/* Bombs and spawn points (enemies appear on them) count as closer. */
			if (spr->type == Bomb) d = (d > 12) ? (UINT16)(d - 12) : 0;
			else if (spr->type == SpawnPoint) d = (d > 6) ? (UINT16)(d - 6) : 0;
			if (d < best_d) {
				best_d = d;
				best = spr;
			}
		}
	}
	if (out_d) *out_d = best_d;
	return best;
}

/* How safe is the point (x, y)? Manhattan distance from that point (as the
 * player's centre) to the nearest hazard centre; bombs and spawn points count
 * as closer. Capped at 96. Read-only. */
static UINT8 e2e_danger_spawns = 1; /* errands ignore spawn points */

static UINT16 E2E_DangerAt(UINT16 x, UINT16 y) {
	UINT8 i;
	Sprite* spr;
	UINT16 best = 96;
	for (i = 0; i != e2e_nhz; ++i) {
		UINT16 d;
		UINT16 sx;
		UINT16 sy;
		spr = e2e_hz[i];
		if (spr->type == SpawnPoint && (enemies_left_to_spawn == 0 || !e2e_danger_spawns)) continue;
		sx = (UINT16)(spr->x + (spr->coll_w >> 1));
		sy = (UINT16)(spr->y + (spr->coll_h >> 1));
		d = (UINT16)Abs16((INT16)x - (INT16)sx) + (UINT16)Abs16((INT16)y - (INT16)sy);
		if (spr->type != Bomb && spr->type != SpawnPoint) {
			/* Contact = hitboxes overlap on BOTH axes (player 16x16 vs enemy
			 * box), so the real gap is the larger axis distance minus the two
			 * half-sizes. Manhattan said "28 px" for a diagonal enemy that was
			 * already touching. Scale x2 to stay comparable to the old units. */
			INT16 adx = Abs16((INT16)x - (INT16)sx);
			INT16 ady = Abs16((INT16)y - (INT16)sy);
			INT16 gap = (adx > ady ? adx : ady) - (INT16)(8 + (spr->coll_w >> 1));
			d = gap <= 0 ? 0 : (UINT16)(gap * 2 + 4);
		}
		if (e2e_hz_path[i] && d >= 40) { /* up close the (lagging) flow field can't be trusted */
			/* Lower bound on the walk from (x, y): walk(player->enemy) minus
			 * how far (x, y) is from the player. */
			UINT16 off = (UINT16)Abs16((INT16)x - (INT16)e2e_hz_px) + (UINT16)Abs16((INT16)y - (INT16)e2e_hz_py);
			if (e2e_hz_path[i] > off + 8 && e2e_hz_path[i] - off - 8 > d) d = e2e_hz_path[i] - off - 8;
		}
		if (spr->type == ChargeVirus && spr->custom_data[4] != 0) {
			/* ChargeVirus.c: state 1 = winding up (the dash goes along whichever
			 * axis is farther from the player when it fires), 2 = dashing with
			 * custom_data[6]/[7] = step. Its lane (±24 px: both 16 px bodies
			 * plus slack), up to a dash length ahead, is deadly. */
			INT16 ax = (INT16)x - (INT16)sx;
			INT16 ay = (INT16)y - (INT16)sy;
			if (spr->custom_data[4] == 2) {
				INT8 sdx = (INT8)spr->custom_data[6];
				INT8 sdy = (INT8)spr->custom_data[7];
				if (sdx && Abs16(ay) < 24 && (sdx > 0 ? ax > -16 : ax < 16) && Abs16(ax) < 64) d = 0;
				if (sdy && Abs16(ax) < 24 && (sdy > 0 ? ay > -16 : ay < 16) && Abs16(ay) < 64) d = 0;
			} else {
				/* Winding up: it will dash along the axis we're farther away on. */
				if (Abs16(ax) >= Abs16(ay)) { if (Abs16(ay) < 24 && Abs16(ax) < 64) d = 0; }
				else if (Abs16(ax) < 24 && Abs16(ay) < 64) d = 0;
			}
		}
		if (spr->type == Bomb) d = (d > 16) ? (UINT16)(d - 16) : 0;
		else if (spr->type == SpawnPoint) {
			/* Flashing = an enemy appears there within a second: keep clear. */
			UINT8 pad = spr->custom_data[CD_SPAWN_WARNING] ? 32 : 8;
			d = (d > pad) ? (UINT16)(d - pad) : 0;
		}
		if (d < best) best = d;
	}
	return best;
}

/* Closed door nearest to (x, y), or 0. */
static Sprite* E2E_ClosedDoorNear(UINT16 x, UINT16 y, UINT16 max_d) {
	UINT8 i;
	Sprite* spr;
	Sprite* best = 0;
	UINT16 best_d = max_d;
	SPRITEMANAGER_ITERATE(i, spr) {
		UINT16 d;
		if (!IsDoorType(spr->type) || spr->custom_data[CD_DOOR_STATE] != 0) continue;
		d = (UINT16)Abs16((INT16)x - (INT16)spr->x) + (UINT16)Abs16((INT16)y - (INT16)spr->y);
		if (d <= best_d) {
			best_d = d;
			best = spr;
		}
	}
	return best;
}

/* ------------------------------------------------------------ movement */

/* Call once per bot frame BEFORE choosing keys: did last frame's input move us? */
static void E2E_TrackStuck(Sprite* p) {
	if (!p) return;
	if ((e2e_keys & DIR_KEYS) && p->x == e2e_last_x && p->y == e2e_last_y) {
		if (e2e_stuck < 255) e2e_stuck++;
	} else {
		e2e_stuck = 0;
	}
	e2e_last_x = p->x;
	e2e_last_y = p->y;
}

static UINT8 E2E_KeysToward(Sprite* p, UINT16 tx, UINT16 ty, UINT8 reach) {
	INT16 dx = (INT16)tx - (INT16)p->x;
	INT16 dy = (INT16)ty - (INT16)p->y;
	UINT8 k = 0;
	if (dx > (INT16)reach) k |= J_RIGHT;
	else if (dx < -(INT16)reach) k |= J_LEFT;
	if (dy > (INT16)reach) k |= J_DOWN;
	else if (dy < -(INT16)reach) k |= J_UP;
	return k;
}

/* Greedy walk + wall-follow detour when blocked. Returns held keys. */
static UINT8 E2E_Walk(Sprite* p, UINT16 tx, UINT16 ty, UINT8 reach) {
	UINT8 k;
	if (!p) return 0;
	k = E2E_KeysToward(p, tx, ty, reach);
	if (!k) {
		e2e_detour_frames = 0;
		e2e_detour_len = 0;
		return 0;
	}
	if (e2e_detour_frames) {
		e2e_detour_frames--;
		return e2e_detour_keys;
	}
	if (e2e_stuck > 3) {
		e2e_detour_flip ^= 1;
		if (e2e_detour_len < 96) e2e_detour_len += 16;
		if ((k & (J_LEFT | J_RIGHT)) && (k & (J_UP | J_DOWN))) {
			/* Corner: back off one axis. */
			e2e_detour_keys = e2e_detour_flip
				? (UINT8)((k & (J_UP | J_DOWN)) | ((k & J_LEFT) ? J_RIGHT : J_LEFT))
				: (UINT8)((k & (J_LEFT | J_RIGHT)) | ((k & J_UP) ? J_DOWN : J_UP));
		} else if (k & (J_LEFT | J_RIGHT)) {
			e2e_detour_keys = (UINT8)(k | (e2e_detour_flip ? J_UP : J_DOWN));
		} else {
			e2e_detour_keys = (UINT8)(k | (e2e_detour_flip ? J_LEFT : J_RIGHT));
		}
		e2e_detour_frames = e2e_detour_len;
		e2e_stuck = 0;
		return e2e_detour_keys;
	}
	return k;
}

/* Open ground to fall back to when cornered / idle: left-centre of the
 * visible screen (rooms put walls + doors on the right). */
static UINT16 E2E_HomeX(void) { return (UINT16)(scroll_x + 56); }
static UINT16 E2E_HomeY(void) { return (UINT16)(scroll_y + 64); }

/*
 * Escape planner. For each of 8 directions, walk a "ghost" copy of the
 * player's collision box (x, y, coll_w, coll_h only) through the room map with
 * the same SafeTranslateSprite the player uses, 4 px at a time, and score the
 * route by how open it is and how far from hazards it stays. Nothing in the
 * game is moved or changed; this is the bot "looking" before it runs, so it
 * stops backing itself into dead-end corners.
 */
static const INT8 e2e_dir_x[8] = { 0, 0, -1, 1, -1, 1, -1, 1 };
static const INT8 e2e_dir_y[8] = { -1, 1, 0, 0, -1, -1, 1, 1 };
static const UINT8 e2e_dir_k[8] = {
	J_UP, J_DOWN, J_LEFT, J_RIGHT,
	J_UP | J_LEFT, J_UP | J_RIGHT, J_DOWN | J_LEFT, J_DOWN | J_RIGHT
};
static UINT8 e2e_escape_keys;
static UINT8 e2e_escape_hold;

static UINT8 E2E_PlanEscape(Sprite* p) {
	Sprite ghost;
	UINT8 d;
	UINT8 s;
	UINT8 best_k = 0;
	UINT16 best_score = 0;
	UINT8 cw = (UINT8)(p->coll_w >> 1);
	UINT8 ch = (UINT8)(p->coll_h >> 1);

	for (d = 0; d != 8; ++d) {
		UINT16 moved = 0;
		UINT16 worst = 96;
		UINT16 score;
		ghost.x = p->x;
		ghost.y = p->y;
		ghost.coll_w = p->coll_w;
		ghost.coll_h = p->coll_h;
		for (s = 0; s != 6; ++s) {
			UINT16 dz;
			if (e2e_dir_x[d] && !SafeTranslateSprite(&ghost, (INT8)(e2e_dir_x[d] * 4), 0)) moved += 4;
			if (e2e_dir_y[d] && !SafeTranslateSprite(&ghost, 0, (INT8)(e2e_dir_y[d] * 4))) moved += 4;
			dz = E2E_DangerAt((UINT16)(ghost.x + cw), (UINT16)(ghost.y + ch));
			if (dz < worst) worst = dz;
		}
		/* Safety along the whole route matters most; then room to keep moving. */
		score = (UINT16)(worst * 3 + E2E_DangerAt((UINT16)(ghost.x + cw), (UINT16)(ghost.y + ch)) + moved);
		if (moved == 0) score = 0;
		if (score > best_score) {
			best_score = score;
			best_k = e2e_dir_k[d];
		}
	}
	return best_k;
}

#if E2E_USES_PLANNER
static UINT8 E2E_LOSFrom(UINT16 x, UINT16 y, Sprite* e);
static Sprite* E2E_NearestEnemy(Sprite* from, UINT16* out_d);
/*
 * Kiting planner (rooms): enemies are slow (1 px per 5-21 updates, the player
 * 1 px per update), so out-walk them down corridors and keep shooting. For
 * each direction, walk up to 4 tiles on the precomputed walk map and score:
 * safety along the way (path-aware danger, Charge dash lanes), room to keep
 * going (dead ends are death traps), and a firing lane on the nearest enemy
 * when the gun is nearly reloaded. Standing still is scored the same way.
 */
/* Open space around tile (tx, ty): walkable edges in the 5x5 tiles around
 * it (0-50). Big = middle of open ground; small = pocket, corridor, corner. */
static UINT8 E2E_Space(UINT8 tx, UINT8 ty) {
	UINT8 n = 0;
	UINT8 x0 = tx >= 2 ? (UINT8)(tx - 2) : 0;
	UINT8 y0 = ty >= 2 ? (UINT8)(ty - 2) : 0;
	UINT8 x;
	UINT8 y;
	for (y = y0; y != (UINT8)(ty + 3) && y < rt_h; ++y) {
		UINT8* w = &rt_walk[rt_row[y] + x0];
		for (x = x0; x != (UINT8)(tx + 3) && x < rt_w; ++x, ++w) {
			if (*w & 1) n++;
			if (*w & 2) n++;
		}
	}
	return n;
}

/* Range to the nearest enemy (manhattan, px) as a score: stay where shots
 * land (range 88) but outside contact range. */
static INT16 E2E_RangeScore(UINT16 x, UINT16 y, Sprite* e) {
	UINT16 dd;
	if (!e) return 0;
	dd = (UINT16)Abs16((INT16)x - (INT16)e->x) + (UINT16)Abs16((INT16)y - (INT16)e->y);
	if (dd >= 36 && dd <= 88) return 30;
	if (dd > 120) return -20;
	return 0;
}

/* Walk up to n tiles from (*tx, *ty) in direction d on the walk map. Tracks
 * the worst danger seen; *walled = stopped by a wall. Returns tiles moved. */
static const INT8 kdx[4] = { 0, 0, -1, 1 };
static const INT8 kdy[4] = { -1, 1, 0, 0 };
static const UINT8 kk[4] = { J_UP, J_DOWN, J_LEFT, J_RIGHT };
static UINT8 kite_cw;
static UINT8 kite_ch;
static UINT16 kite_here;
static UINT8 E2E_KiteWalk(UINT8* tx, UINT8* ty, UINT8 d, UINT8 n, UINT16* worst, UINT8* walled) {
	UINT8 moved = 0;
	while (moved != n) {
		UINT16 idx = rt_row[*ty] + *tx;
		UINT8 ok;
		UINT16 dz;
		if (d == 0) ok = *ty && (rt_walk[idx - rt_w] & 2);
		else if (d == 1) ok = rt_walk[idx] & 2;
		else if (d == 2) ok = *tx && (rt_walk[idx - 1] & 1);
		else ok = rt_walk[idx] & 1;
		if (ok && rt_ndoors && RT_DoorTile((UINT8)(*tx + kdx[d]), (UINT8)(*ty + kdy[d]))) ok = 0; /* closed door */
		if (!ok) { *walled = 1; break; }
		*tx = (UINT8)(*tx + kdx[d]);
		*ty = (UINT8)(*ty + kdy[d]);
		moved++;
		dz = E2E_DangerAt((UINT16)(((UINT16)*tx << 3) + kite_cw), (UINT16)(((UINT16)*ty << 3) + kite_ch));
		if (dz < *worst) *worst = dz;
		if (dz < 12 && dz < kite_here) break; /* walking into it */
	}
	return moved;
}

static UINT8 E2E_PlanKite(Sprite* p) {
	UINT8 cx = (UINT8)((p->x + 4) >> 3);
	UINT8 cy = (UINT8)((p->y + 4) >> 3);
	Sprite* e = E2E_NearestEnemy(p, 0);
	UINT8 reloading_soon = shoot_cooldown <= 30;
	INT16 best;
	UINT8 best_d = 0xFF;
	UINT8 d;
	UINT8 leg;
	UINT16 dz;
	if (cx >= rt_w || cy >= rt_h) return E2E_PlanEscape(p);
	kite_cw = (UINT8)(p->coll_w >> 1);
	kite_ch = (UINT8)(p->coll_h >> 1);
	/* stay: no room-to-move bonus, so moving wins ties */
	dz = E2E_DangerAt((UINT16)(p->x + kite_cw), (UINT16)(p->y + kite_ch));
	kite_here = dz;
	best = (INT16)((dz > 80 ? 80 : dz) * 5 + E2E_Space(cx, cy) * 3 + E2E_RangeScore(p->x, p->y, e));
	if (e && E2E_LOSFrom(p->x, p->y, e)) best += reloading_soon ? 80 : 25;
	if (dz < 16) best -= 300; /* in contact range / a dash lane: never just stand there */
	/* Candidates per first direction d: straight (4 tiles), or 2 tiles then
	 * a turn either way (3 tiles): circling around an enemy, not just away. */
	for (d = 0; d != 4; ++d) {
		for (leg = 0; leg != 3; ++leg) {
			UINT8 tx = cx;
			UINT8 ty = cy;
			UINT8 walled = 0;
			UINT16 worst = 96;
			UINT8 moved;
			INT16 score;
			if (leg == 0) {
				moved = E2E_KiteWalk(&tx, &ty, d, 4, &worst, &walled);
			} else {
				UINT8 d2 = d < 2 ? (UINT8)(leg + 1) : (UINT8)(leg - 1); /* perpendicular */
				moved = E2E_KiteWalk(&tx, &ty, d, 2, &worst, &walled);
				if (!moved || walled) continue;
				moved = (UINT8)(moved + E2E_KiteWalk(&tx, &ty, d2, 3, &worst, &walled));
			}
			if (!moved) continue;
			dz = E2E_DangerAt((UINT16)(((UINT16)tx << 3) + kite_cw), (UINT16)(((UINT16)ty << 3) + kite_ch));
			/* Safety along the way and at the end dominate; then open ground
			 * (no corners), shooting range, a lined-up shot when reloaded. */
			score = (INT16)((worst > 80 ? 80 : worst) * 2 + (dz > 80 ? 80 : dz) * 5 + moved * 4);
			if (dz < 16) score -= 200; /* ends in danger */
			{
				/* Passing through contact range costs, but only danger WORSE
				 * than where we stand: stepping out of a dash lane starts in
				 * the lane too. */
				UINT16 ref = kite_here < 24 ? kite_here : 24;
				if (worst < ref) score -= (INT16)((ref - worst) * 15);
			}
			score += (INT16)(E2E_Space(tx, ty) * 3);
			score += E2E_RangeScore((UINT16)tx << 3, (UINT16)ty << 3, e);
			if (walled && moved < 3) score -= 30;
			/* Commit to the last choice unless something is clearly better. */
			if (e2e_escape_keys & kk[d]) score += 40;
			if (e && E2E_LOSFrom((UINT16)tx << 3, (UINT16)ty << 3, e)) score += reloading_soon ? 80 : 25;
			if (score > best) {
				best = score;
				best_d = d;
			}
		}
	}
	if (best_d == 0xFF) return 0;
	{
		UINT8 k = kk[best_d];
		/* slide the other axis onto the grid so corridors fit */
		if (kdx[best_d]) { if (p->y < ((UINT16)cy << 3)) k |= J_DOWN; else if (p->y > ((UINT16)cy << 3)) k |= J_UP; }
		else { if (p->x < ((UINT16)cx << 3)) k |= J_RIGHT; else if (p->x > ((UINT16)cx << 3)) k |= J_LEFT; }
		return k;
	}
}
#endif

/* Get away from danger: re-plan every few frames (or at once if blocked). */
static UINT8 E2E_Flee(Sprite* p, UINT16 x, UINT16 y) {
	(void)x;
	(void)y;
	if (!e2e_escape_hold || e2e_stuck > 1) {
#if E2E_USES_PLANNER
		if (current_state == StateGame && rt_room == current_room) {
			e2e_escape_keys = E2E_PlanKite(p);
			e2e_escape_hold = 3;
			e2e_stuck = 0;
			return e2e_escape_keys;
		}
#endif
		e2e_escape_keys = E2E_PlanEscape(p);
		e2e_escape_hold = 6;
		e2e_stuck = 0;
	} else {
		e2e_escape_hold--;
	}
	return e2e_escape_keys;
}

/*
 * Line of sight: simulate the shot. Projectiles spawn at the player's x,y as
 * an 8x8 box and move with SafeTranslateSprite (stopping at walls), so walk a
 * ghost 8x8 box 2 px at a time the same way and see whether it overlaps `e`
 * before a wall stops it. Read-only. Returns the facing key, or 0.
 */
#if E2E_USES_PLANNER
/* Level tests: read line of sight off the room's wall bitmap (RT_LOSFast,
 * cross-checked against the simulation below: no missed shots, ~1% extra
 * "clear" at the range limit). ~5x cheaper, and keeps E2E.o in one bank. */
static UINT8 E2E_LOSFrom(UINT16 x, UINT16 y, Sprite* e) {
	UINT8 ok;
	UINT8 k = RT_LOSFast(x, y, e, &ok);
	if (ok) return k;
	/* Boss run / boss fight (no room bitmap): open arenas, lane test only. */
	if ((INT16)y + 8 > (INT16)e->y && (INT16)y < (INT16)(e->y + e->coll_h))
		return e->x + (e->coll_w >> 1) > x + 4 ? J_RIGHT : J_LEFT;
	if ((INT16)x + 8 > (INT16)e->x && (INT16)x < (INT16)(e->x + e->coll_w))
		return e->y + (e->coll_h >> 1) > y + 4 ? J_DOWN : J_UP;
	return 0;
}
#else
static UINT8 E2E_LOSFrom(UINT16 x, UINT16 y, Sprite* e) {
	static const INT8 ddx[4] = { 0, 0, -1, 1 };
	static const INT8 ddy[4] = { -1, 1, 0, 0 };
	static const UINT8 dk[4] = { J_UP, J_DOWN, J_LEFT, J_RIGHT };
	INT16 ex0 = (INT16)e->x;
	INT16 ey0 = (INT16)e->y;
	INT16 ex1 = (INT16)(e->x + e->coll_w);
	INT16 ey1 = (INT16)(e->y + e->coll_h);
	UINT8 d;
	for (d = 0; d != 4; ++d) {
		Sprite g;
		UINT8 s;
		if (ddx[d] == 0) {
			if (!((INT16)x + 8 > ex0 && (INT16)x < ex1)) continue;          /* column lane */
			if (ddy[d] < 0 ? ey0 >= (INT16)y : ey1 <= (INT16)y) continue;  /* wrong side */
		} else {
			if (!((INT16)y + 8 > ey0 && (INT16)y < ey1)) continue;          /* row lane */
			if (ddx[d] < 0 ? ex0 >= (INT16)x : ex1 <= (INT16)x) continue;
		}
		g.x = x;
		g.y = y;
		g.coll_w = 8;
		g.coll_h = 8;
		/* A tile (8 px) per step: the box's leading edge still visits every
		 * tile column/row, so no wall is skipped, at 1/4 the cost of 2 px
		 * steps. At a wall, finish in the projectile's real 2 px steps so an
		 * enemy hugging the wall still counts. Range = 88 px, as before. */
		for (s = 0; s != 11; ++s) {
			if ((INT16)g.x + 8 > ex0 && (INT16)g.x < ex1 && (INT16)g.y + 8 > ey0 && (INT16)g.y < ey1) return dk[d];
			if (SafeTranslateSprite(&g, (INT8)(ddx[d] * 8), (INT8)(ddy[d] * 8))) {
				UINT8 f;
				for (f = 0; f != 3; ++f) {
					if (SafeTranslateSprite(&g, (INT8)(ddx[d] * 2), (INT8)(ddy[d] * 2))) break;
					if ((INT16)g.x + 8 > ex0 && (INT16)g.x < ex1 && (INT16)g.y + 8 > ey0 && (INT16)g.y < ey1) return dk[d];
				}
				break; /* wall */
			}
		}
	}
	return 0;
}
#endif

/* Would a shot fired now along a row/column actually pass through `e`?
 * Uses real hitbox overlap (shots are ~8 px wide), so big enemies count when
 * any part of them is in the lane. Fills the key that faces it. */
static UINT8 E2E_Aligned(Sprite* p, Sprite* e, UINT8* face) {
	UINT8 k = E2E_LOSFrom(p->x, p->y, e);
	if (k) {
		*face = k;
		return 1;
	}
	return 0;
}

static UINT8 E2E_Nav(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, Sprite* want);

/*
 * A threat is close but not lined up: is there a firing position a few
 * pixels to the side (reachable without hitting a wall)? Returns the d-pad
 * key that steps toward it, or 0. A human just nudges sideways and shoots
 * instead of standing still or backing into a dead end.
 */
static UINT8 E2E_SidestepShot(Sprite* p, Sprite* e) {
	static const INT8 offs[6] = { 2, -2, 4, -4, 6, -6 };
	UINT8 i;
	for (i = 0; i != 6; ++i) {
		Sprite g;
		INT8 o = offs[i];
		g.coll_w = p->coll_w;
		g.coll_h = p->coll_h;
		/* sideways in x */
		g.x = p->x;
		g.y = p->y;
		if (!SafeTranslateSprite(&g, o, 0) && E2E_LOSFrom(g.x, g.y, e)) return o > 0 ? J_RIGHT : J_LEFT;
		/* sideways in y */
		g.x = p->x;
		g.y = p->y;
		if (!SafeTranslateSprite(&g, 0, o) && E2E_LOSFrom(g.x, g.y, e)) return o > 0 ? J_DOWN : J_UP;
	}
	return 0;
}

/*
 * Fight with real shots: if any enemy is lined up and the gun is ready, press
 * "face it + B" for one frame (that turns the player and fires). Otherwise
 * evade the nearest hazard (enemies, bombs) when it is close, else line up on
 * the nearest enemy's row/column at a safe distance.
 */
static UINT8 E2E_Fight(Sprite* p, Sprite* e, UINT8 gun_ready) {
	UINT8 i;
	Sprite* spr;
	Sprite* hz;
	UINT16 hd;
	UINT8 face;
	INT16 dx;
	INT16 dy;
	INT16 along;
	INT16 perp;
	UINT8 toward;
	UINT8 away;
	UINT8 perp_key;
	UINT8 k = 0;

	if (!p) return 0;

	if (gun_ready) {
		SPRITEMANAGER_ITERATE(i, spr) {
			if (!IsEnemyType(spr->type) && spr->type != Boss) continue;
			if (E2E_Aligned(p, spr, &face)) return (UINT8)(face | J_B);
		}
	}

	hz = E2E_NearestHazard(p, &hd);
	e2e_dbg2 = (UINT8)E2E_DangerAt((UINT16)(p->x + (p->coll_w >> 1)), (UINT16)(p->y + (p->coll_h >> 1)));
#if E2E_USES_PLANNER
	if (current_state == StateGame && rt_room == current_room) {
		/* Rooms: one combat controller (E2E_PlanKite) positions us - safe,
		 * open, at shooting range, lined up when the gun reloads. Only a
		 * far-away enemy is hunted down by route. */
		if (e && E2E_Dist(p, e->x, e->y) > 120 && e2e_dbg2 >= 40) return E2E_Nav(p, e->x, e->y, 40, 0);
		e2e_dbg = (UINT8)(e2e_dbg | 0x20);
		return E2E_Flee(p, 0, 0);
	}
#endif
	if (hz && e2e_dbg2 < 30) {
		/* Not yet touching and the gun is (nearly) ready: nudge into a
		 * firing lane instead of running. */
		if (e && e2e_dbg2 >= 12 && shoot_cooldown < 6) {
			k = E2E_SidestepShot(p, e);
			if (k) { e2e_dbg = (UINT8)(e2e_dbg | 0x40); return k; }
		}
		/* Reloading with the enemy already in our lane: back straight away
		 * down that lane (kiting), so the shot is lined up the moment the
		 * gun is ready, instead of fleeing sideways out of the lane. */
#if !E2E_USES_PLANNER /* rooms with the planner use E2E_PlanKite (dead-end aware) */
		if (e && hz == e && shoot_cooldown >= 6 && E2E_Aligned(p, e, &face)) {
			Sprite g;
			INT8 bx = face == J_LEFT ? 4 : face == J_RIGHT ? -4 : 0;
			INT8 by = face == J_UP ? 4 : face == J_DOWN ? -4 : 0;
			g.x = p->x;
			g.y = p->y;
			g.coll_w = p->coll_w;
			g.coll_h = p->coll_h;
			if (!SafeTranslateSprite(&g, bx, by) && !SafeTranslateSprite(&g, bx, by)) {
				e2e_dbg = (UINT8)(e2e_dbg | 0x80);
				return bx > 0 ? J_RIGHT : bx < 0 ? J_LEFT : by > 0 ? J_DOWN : J_UP;
			}
		}
#endif
		e2e_dbg = (UINT8)(e2e_dbg | 0x20);
		k = E2E_Flee(p, hz->x, hz->y);
		if (k) return k;
#if E2E_USES_PLANNER
		if (current_state == StateGame) return 0; /* kite planner chose to hold still */
#endif
		/* Nowhere safer to go: stand and fight (fall through). */
	}
	e2e_escape_hold = 0;

	if (!e) return E2E_Walk(p, E2E_HomeX(), E2E_HomeY(), 10); /* idle: wait in the open */
	dx = (INT16)(e->x + (e->coll_w >> 1)) - (INT16)(p->x + (p->coll_w >> 1));
	dy = (INT16)(e->y + (e->coll_h >> 1)) - (INT16)(p->y + (p->coll_h >> 1));

	if (Abs16(dx) <= Abs16(dy)) {
		along = dy;
		perp = dx;
		toward = dy > 0 ? J_DOWN : J_UP;
		away = dy > 0 ? J_UP : J_DOWN;
		perp_key = dx > 0 ? J_RIGHT : J_LEFT;
	} else {
		along = dx;
		perp = dy;
		toward = dx > 0 ? J_RIGHT : J_LEFT;
		away = dx > 0 ? J_LEFT : J_RIGHT;
		perp_key = dy > 0 ? J_DOWN : J_UP;
	}

	if (Abs16(dx) + Abs16(dy) > 72) {
		/* Far / maybe stuck behind walls (often off-screen): go hunt it. */
		return E2E_Nav(p, e->x, e->y, 40, 0);
	}
	if (Abs16(perp) > 3) k |= perp_key;
	if (Abs16(along) < 36) k |= away;
	else if (Abs16(along) > 64) k |= toward;

	if (e2e_stuck > 3) {
		/* Backed into a wall: pick the most open, safest direction. */
		k = E2E_PlanEscape(p);
		e2e_stuck = 0;
	}
	return k;
}

/*
 * SpritePlayer buys ANY closed door it stands within 17 px of (if it can pay).
 * A human walks around doors they don't want; so does the bot: drop any
 * direction that would bring it into buying range of a closed door other than
 * the one it is deliberately walking to (e2e_door_target).
 */
static Sprite* e2e_door_target;

static UINT8 E2E_AvoidDoors(Sprite* p, UINT8 k) {
	UINT8 i;
	Sprite* spr;
	UINT8 orig = k;
	if (!p || !(k & DIR_KEYS)) return k;
	SPRITEMANAGER_ITERATE(i, spr) {
		INT16 dx;
		INT16 dy;
		if (spr->type == NextLevelPortal && e2e_skip_portal) {
			/* Not leaving yet (a power-up is still here): don't brush the portal. */
			dx = (INT16)spr->x - (INT16)p->x;
			dy = (INT16)spr->y - (INT16)p->y;
			if (Abs16(dy) <= 20 && Abs16(dx) <= 24) {
				if (dx > 0) k &= (UINT8)~J_RIGHT;
				else if (dx < 0) k &= (UINT8)~J_LEFT;
			}
			if (Abs16(dx) <= 20 && Abs16(dy) <= 24) {
				if (dy > 0) k &= (UINT8)~J_DOWN;
				else if (dy < 0) k &= (UINT8)~J_UP;
			}
			continue;
		}
		if (!IsDoorType(spr->type) || spr->custom_data[CD_DOOR_STATE] != 0) continue;
		if (spr == e2e_door_target) continue;
		if (ready_coins < spr->custom_data[CD_DOOR_COST]) continue; /* can't buy it: harmless */
		dx = (INT16)spr->x - (INT16)p->x;
		dy = (INT16)spr->y - (INT16)p->y;
		if (Abs16(dy) <= 22 && Abs16(dx) <= 26) {
			if (dx > 0) k &= (UINT8)~J_RIGHT;
			else if (dx < 0) k &= (UINT8)~J_LEFT;
		}
		if (Abs16(dx) <= 22 && Abs16(dy) <= 26) {
			if (dy > 0) k &= (UINT8)~J_DOWN;
			else if (dy < 0) k &= (UINT8)~J_UP;
		}
		/* Everything got filtered: step directly away along the safer axis. */
		if (!(k & DIR_KEYS) && (orig & DIR_KEYS)) {
			if (Abs16(dx) <= Abs16(dy)) k |= (dy > 0) ? J_UP : J_DOWN;
			else k |= (dx > 0) ? J_LEFT : J_RIGHT;
		}
	}
	return k;
}

/* ------------------------------------------------------------ navigation */

/*
 * Breadth-first path search over the visible screen on an 8 px grid aligned
 * to the player's current position. Edges are tested by sliding a "ghost"
 * hitbox (x, y, coll_w, coll_h only) through SafeTranslateSprite -- the same
 * wall test the player uses -- so it only READS the room map. Cells within
 * buying range of a closed door we don't want are treated as blocked. The
 * bot then holds the d-pad toward the next waypoint.
 */
#if !E2E_USES_PLANNER /* level tests use the whole-map planner (RT_*) instead; saves ~1.2 KB RAM */
#define NAV_W 20
#define NAV_H 18
#define NAV_MAX_PATH 64
#define NAV_DOOR_KEEP_OUT 26

static UINT8 nav_from[NAV_H][NAV_W]; /* 0 unseen, 1..4 came-by dir+1, 5 start, 6 blocked */
static UINT8 nav_qx[NAV_W * NAV_H];
static UINT8 nav_qy[NAV_W * NAV_H];
static UINT8 nav_px[NAV_MAX_PATH];
static UINT8 nav_py[NAV_MAX_PATH];
static UINT8 nav_len;
static UINT8 nav_pos;
static UINT16 nav_gx0;
static UINT16 nav_gy0;
static UINT16 nav_tx = 0xFFFF;
static UINT16 nav_ty = 0xFFFF;
static UINT16 nav_age;
static const INT8 nav_dx[4] = { 0, 0, -1, 1 };
static const INT8 nav_dy[4] = { -1, 1, 0, 0 };

static UINT8 E2E_NavBlocked(UINT16 x, UINT16 y, Sprite* want) {
	UINT8 i;
	Sprite* spr;
	SPRITEMANAGER_ITERATE(i, spr) {
		if (!IsDoorType(spr->type) || spr->custom_data[CD_DOOR_STATE] != 0) continue;
		if (spr == want) continue;
		if (Abs16((INT16)x - (INT16)spr->x) <= NAV_DOOR_KEEP_OUT &&
		    Abs16((INT16)y - (INT16)spr->y) <= NAV_DOOR_KEEP_OUT) return 1;
	}
	return 0;
}

static UINT8 E2E_NavGoal(UINT16 x, UINT16 y, UINT16 tx, UINT16 ty, UINT8 r) {
	return Abs16((INT16)x - (INT16)tx) <= (INT16)r && Abs16((INT16)y - (INT16)ty) <= (INT16)r;
}

static UINT8 E2E_NavPlan(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, Sprite* want) {
	Sprite ghost;
	UINT8 pcx;
	UINT8 pcy;
	UINT16 head = 0;
	UINT16 tail = 0;
	UINT8 found = 0;
	UINT8 fx = 0;
	UINT8 fy = 0;
	UINT8 cx;
	UINT8 cy;
	UINT8 d;
	UINT8 len;
	UINT8 i;

	/* Grid centred on the player (not the screen) so off-screen targets in
	 * scrolling rooms are reachable; clamp at the map's top/left edge. */
	pcx = (UINT8)(NAV_W / 2);
	pcy = (UINT8)(NAV_H / 2);
	if ((p->x >> 3) < pcx) pcx = (UINT8)(p->x >> 3);
	if ((p->y >> 3) < pcy) pcy = (UINT8)(p->y >> 3);
	nav_gx0 = (UINT16)(p->x - ((UINT16)pcx << 3));
	nav_gy0 = (UINT16)(p->y - ((UINT16)pcy << 3));
	nav_tx = tx;
	nav_ty = ty;
	nav_age = 0;
	nav_len = 0;
	nav_pos = 0;

	memset(nav_from, 0, sizeof(nav_from));
	nav_from[pcy][pcx] = 5;
	nav_qx[tail] = pcx;
	nav_qy[tail] = pcy;
	tail++;
	ghost.coll_w = p->coll_w;
	ghost.coll_h = p->coll_h;

	while (head != tail) {
		UINT16 x;
		UINT16 y;
		cx = nav_qx[head];
		cy = nav_qy[head];
		head++;
		x = (UINT16)(nav_gx0 + ((UINT16)cx << 3));
		y = (UINT16)(nav_gy0 + ((UINT16)cy << 3));
		if (E2E_NavGoal(x, y, tx, ty, r)) {
			found = 1;
			fx = cx;
			fy = cy;
			break;
		}
		for (d = 0; d != 4; ++d) {
			INT8 nx = (INT8)(cx + nav_dx[d]);
			INT8 ny = (INT8)(cy + nav_dy[d]);
			if (nx < 0 || ny < 0 || nx >= NAV_W || ny >= NAV_H) continue;
			if (nav_from[ny][nx]) continue;
			ghost.x = x;
			ghost.y = y;
			if (SafeTranslateSprite(&ghost, (INT8)(nav_dx[d] * 4), (INT8)(nav_dy[d] * 4)) ||
			    SafeTranslateSprite(&ghost, (INT8)(nav_dx[d] * 4), (INT8)(nav_dy[d] * 4))) {
				continue; /* wall between these cells */
			}
			if (E2E_NavBlocked(ghost.x, ghost.y, want)) {
				nav_from[ny][nx] = 6;
				continue;
			}
			nav_from[ny][nx] = (UINT8)(d + 1);
			nav_qx[tail] = (UINT8)nx;
			nav_qy[tail] = (UINT8)ny;
			tail++;
		}
	}
	if (!found) return 0;

	/* Path length, then fill waypoints start->goal (first NAV_MAX_PATH only). */
	len = 0;
	cx = fx;
	cy = fy;
	while (nav_from[cy][cx] != 5) {
		d = (UINT8)(nav_from[cy][cx] - 1);
		cx = (UINT8)(cx - nav_dx[d]);
		cy = (UINT8)(cy - nav_dy[d]);
		len++;
	}
	cx = fx;
	cy = fy;
	for (i = len; i != 0; --i) {
		if ((UINT8)(i - 1) < NAV_MAX_PATH) {
			nav_px[i - 1] = cx;
			nav_py[i - 1] = cy;
		}
		d = (UINT8)(nav_from[cy][cx] - 1);
		cx = (UINT8)(cx - nav_dx[d]);
		cy = (UINT8)(cy - nav_dy[d]);
	}
	nav_len = len < NAV_MAX_PATH ? len : NAV_MAX_PATH;
	return 1;
}

#if E2E_USES_PLANNER
static UINT8 RT_Go(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, Sprite* want);
#endif

/* Keys to follow a planned path to (tx, ty); replans when needed. */
static UINT8 E2E_Nav(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, Sprite* want) {
	UINT16 wx;
	UINT16 wy;
	if (!p) return 0;
#if E2E_USES_PLANNER
	return RT_Go(p, tx, ty, r, want); /* whole-map planner for the big maze rooms */
#endif
	if (E2E_NavGoal(p->x, p->y, tx, ty, r)) {
		nav_len = 0;
		return E2E_KeysToward(p, tx, ty, 0); /* final step in (door buys itself) */
	}
	/* Replan when the target moved noticeably (enemies move), the path ran
	 * out, we're blocked, or every ~4 s. */
	if (Abs16((INT16)tx - (INT16)nav_tx) > 16 || Abs16((INT16)ty - (INT16)nav_ty) > 16 ||
	    nav_pos >= nav_len || e2e_stuck > 3 || ++nav_age > 240) {
		if (!E2E_NavPlan(p, tx, ty, r, want)) return E2E_Walk(p, tx, ty, 0);
		e2e_stuck = 0;
		if (!nav_len) return E2E_KeysToward(p, tx, ty, 0);
	}
	wx = (UINT16)(nav_gx0 + ((UINT16)nav_px[nav_pos] << 3));
	wy = (UINT16)(nav_gy0 + ((UINT16)nav_py[nav_pos] << 3));
	if (p->x == wx && p->y == wy) {
		nav_pos++;
		if (nav_pos >= nav_len) return E2E_KeysToward(p, tx, ty, 0);
		wx = (UINT16)(nav_gx0 + ((UINT16)nav_px[nav_pos] << 3));
		wy = (UINT16)(nav_gy0 + ((UINT16)nav_py[nav_pos] << 3));
	}
	return E2E_KeysToward(p, wx, wy, 0);
}

#else
static UINT8 RT_Go(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, Sprite* want);
static UINT8 E2E_Nav(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, Sprite* want) {
	if (current_state != StateGame) return E2E_Walk(p, tx, ty, r); /* boss arenas */
	return RT_Go(p, tx, ty, r, want);
}
#endif /* !E2E_USES_PLANNER */

#if !E2E_USES_PLANNER /* level tests use E2E_LevelBot; keeps E2E.o inside one ROM bank */
/* ------------------------------------------------------------ room bot */

/*
 * Make progress toward an objective in the current room:
 *   - enemy close  -> fight/kite it
 *   - objective behind a closed door -> farm coins by killing, then walk into
 *     the door (SpritePlayer buys it itself when adjacent and coins >= cost)
 *   - otherwise walk to the objective
 */
static UINT8 E2E_RoomBot(Sprite* p, UINT16 ox, UINT16 oy, UINT8 reach) {
	Sprite* enemy;
	Sprite* door;
	Sprite* spr;
	UINT8 i;
	UINT8 face;
	UINT16 ed;
	UINT8 gun_ready = (shoot_cooldown == 0);

	enemy = E2E_NearestEnemy(p, &ed);
	door = E2E_ClosedDoorNear(ox, oy, 48);

	/* Not enough coins for the door in the way: farm kills. */
	if (door && ready_coins < door->custom_data[CD_DOOR_COST]) {
		return E2E_Fight(p, enemy, gun_ready);
	}

	/* On an errand (door / portal / pickup): don't get drawn into fights.
	 * Free shot if something is lined up, dodge only real danger, else go. */
	if (gun_ready) {
		SPRITEMANAGER_ITERATE(i, spr) {
			if (!IsEnemyType(spr->type)) continue;
			if (E2E_Aligned(p, spr, &face) && E2E_Dist(p, spr->x, spr->y) < 48)
				return (UINT8)(face | J_B);
		}
	}
	/* Committed to the errand: only real threats (enemies/bombs, not spawn
	 * points) right on top of us make it dodge, and a dodge runs its full
	 * course (escape_hold) so it can't flip-flop with the path every frame. */
	e2e_danger_spawns = 0;
	if (e2e_escape_hold ||
	    E2E_DangerAt((UINT16)(p->x + (p->coll_w >> 1)), (UINT16)(p->y + (p->coll_h >> 1))) < 14) {
		UINT8 k = E2E_Flee(p, 0, 0);
		e2e_danger_spawns = 1;
		return k;
	}
	e2e_danger_spawns = 1;
	if (door) {
		e2e_door_target = door;
		return E2E_Nav(p, door->x, door->y, 14, door);
	}
	return E2E_Nav(p, ox, oy, reach < 4 ? 4 : reach, 0);
}

/* Head for the next-room portal (it only exists once reachable / spawned). */
static UINT8 E2E_GoToPortal(Sprite* p) {
	Sprite* portal = E2E_FindType(NextLevelPortal);
	UINT16 ed;
	Sprite* enemy;
	if (portal) return E2E_RoomBot(p, portal->x, portal->y, 0);
	/* Portal sprite culled off-screen (rooms scroll): use the map position
	 * from Rooms.c (room0/room1 portals are both at 135,52), like a player
	 * who knows the level layout. */
	if (current_room <= 1) return E2E_RoomBot(p, 135, 52, 0);
	enemy = E2E_NearestEnemy(p, &ed);
	if (enemy) return E2E_Fight(p, enemy, shoot_cooldown == 0);
	return 0;
}

#endif /* !E2E_USES_PLANNER */

#if E2E_USES_PLANNER
static INT8 RT_DoorIndex(Sprite* door) {
	UINT8 i;
	if (!door) return -1;
	for (i = 0; i != rt_ndoors; ++i)
		if (rt_door_x[i] == door->x && rt_door_y[i] == door->y) return (INT8)i;
	return -1;
}

/* Follow a non-passable plan to (tx, ty) (or into `want` door's buy range). */
static UINT8 RT_Go(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, Sprite* want) {
	UINT16 wx;
	UINT16 wy;
	if (!p) return 0;
	if (rt_room != current_room) RT_BuildWalk(p);
	if (!want && Abs16((INT16)tx - (INT16)p->x) <= (INT16)r && Abs16((INT16)ty - (INT16)p->y) <= (INT16)r)
		return E2E_KeysToward(p, tx, ty, 0);
	if (Abs16((INT16)tx - (INT16)rt_tx) > 16 || Abs16((INT16)ty - (INT16)rt_ty) > 16 ||
	    rt_pos >= rt_len || e2e_stuck > 3 || ++rt_age > 240) {
		INT8 wi;
		RT_SnapshotDoors();
		wi = RT_DoorIndex(want);
		rt_tx = tx;
		rt_ty = ty;
		rt_age = 0;
		e2e_stuck = 0;
		e2e_t_plan = RT_Plan(p, tx, ty, r, wi, 0, 0, 0);
		if (!e2e_t_plan) {
			if (rt_throttled) rt_tx = 0xFFFF; /* retry when the budget allows */
			return E2E_Walk(p, tx, ty, 0);
		}
	}
	wx = (UINT16)rt_px[rt_pos] << 3;
	wy = (UINT16)rt_py[rt_pos] << 3;
	while (p->x == wx && p->y == wy) {
		rt_pos++;
		if (rt_pos >= rt_len) return want ? E2E_KeysToward(p, want->x, want->y, 0) : E2E_KeysToward(p, tx, ty, 0);
		wx = (UINT16)rt_px[rt_pos] << 3;
		wy = (UINT16)rt_py[rt_pos] << 3;
	}
	return E2E_KeysToward(p, wx, wy, 0);
}

/* Walk to the nearest tile with a clear (simulated) shot at `e`, >= 24 px away. */
static UINT16 rt_fire_age;
static UINT8 rt_fire_fail; /* frames left before retrying a failed fire-spot search */
static UINT8 RT_GoFire(Sprite* p, Sprite* e) {
	UINT16 wx;
	UINT16 wy;
	if (rt_room != current_room) RT_BuildWalk(p);
	/* A failed search (whole-map BFS + a simulated shot per tile) is the
	 * most expensive thing the bot does; retrying it every frame starved the
	 * game loop (~50 frames per game update in map4). Back off instead. */
	if (rt_fire_fail) {
		rt_fire_fail--;
		return 0;
	}
	if (rt_tx != (0x8000 | e->x) || ++rt_fire_age > 45 || rt_pos >= rt_len || e2e_stuck > 3) {
		RT_SnapshotDoors();
		rt_los_target = e;
		rt_fire_age = 0;
		e2e_stuck = 0;
		if (!RT_Plan(p, e->x, e->y, 0, -2, 0, 0, 0)) {
			if (!rt_throttled) rt_fire_fail = 60;
			return 0;
		}
		rt_tx = 0x8000 | e->x; /* mark: fire-spot plan */
		rt_ty = e->y;
	}
	wx = (UINT16)rt_px[rt_pos] << 3;
	wy = (UINT16)rt_py[rt_pos] << 3;
	while (p->x == wx && p->y == wy) {
		rt_pos++;
		if (rt_pos >= rt_len) return 0;
		wx = (UINT16)rt_px[rt_pos] << 3;
		wy = (UINT16)rt_py[rt_pos] << 3;
	}
	return E2E_KeysToward(p, wx, wy, 0);
}

/* Get as close as we can to `e` (it may be somewhere we can't enter yet);
 * enemies chase the player, so this draws them out where we can shoot. */
static UINT16 rt_lure_age;
static UINT8 RT_GoClosest(Sprite* p, Sprite* e) {
	UINT16 wx;
	UINT16 wy;
	if (rt_tx != (0x4000 | e->x) || ++rt_lure_age > 60 || rt_pos >= rt_len || e2e_stuck > 3) {
		RT_SnapshotDoors();
		rt_lure_age = 0;
		e2e_stuck = 0;
		if (!RT_Plan(p, e->x, e->y, 0, -3, 0, 0, 0)) return 0;
		rt_tx = 0x4000 | e->x;
		rt_ty = e->y;
	}
	if (rt_pos >= rt_len) return 0;
	wx = (UINT16)rt_px[rt_pos] << 3;
	wy = (UINT16)rt_py[rt_pos] << 3;
	while (p->x == wx && p->y == wy) {
		rt_pos++;
		if (rt_pos >= rt_len) return 0;
		wx = (UINT16)rt_px[rt_pos] << 3;
		wy = (UINT16)rt_py[rt_pos] << 3;
	}
	return E2E_KeysToward(p, wx, wy, 0);
}

/* Map knowledge: portal per room (Rooms.c), used if the sprite isn't present. */
static const UINT16 rt_portal_x[5] = { 135, 135, 135, 9 * 8, 46 * 8 };
static const UINT16 rt_portal_y[5] = { 52, 52, 52, 15 * 8, 15 * 8 };

static UINT8 E2E_WaveCleared(void) {
	return enemies_left_to_spawn == 0 && enemies_killed == enemies_to_spawn;
}

static UINT8 rt_scan_age = 200;
static UINT8 rt_still;          /* frames the nearest enemy hasn't moved */
static UINT8 rt_left_prev = 0xFF;
static UINT8 rt_have_hot;
static Sprite rt_hot;           /* fake 16x16 target where enemies last appeared */
static UINT16 rt_coin_x = 0xFFFF;
static UINT16 rt_coin_y;
static UINT8 rt_coin_reach = 0xFF;
static UINT16 rt_still_x;
static UINT16 rt_still_y;
static INT8 rt_block = -1;        /* door index (in last snapshot) blocking the route */
static UINT16 rt_block_x;
static UINT16 rt_block_y;
static UINT8 rt_route_cost;

/*
 * Level bot: get from wherever we are to this room's portal.
 *  1. Every ~1.5 s, plan a door-ignoring route to the portal: which closed
 *     doors does it cross, and what do they cost in total?
 *  2. Farm kills (and grab coin pickups when it's quiet) until that total is
 *     banked, so the doors can be opened back to back.
 *  3. Leave on an errand only when it's quiet (wave cleared, or nothing
 *     close): walk to the first blocking door (the player buys it on
 *     contact), repeat, then walk into the portal. Dodge only real danger.
 */
/* Coins to have in the bank before taking the portal out of each room
 * (farming in the early rooms, with the electric attack, is cheaper than in
 * the mazes): 20 covers map4's P1 + P2, the doors in front of the chip. */
static const UINT8 e2e_farm_reserve[5] = { 0, 0, 0, 0, 0 };

static UINT8 E2E_LevelBot(Sprite* p) {
	UINT8 going_for_pickup = 0;
	UINT16 ox;
	UINT16 oy;
	Sprite* portal;
	Sprite* enemy;
	Sprite* door = 0;
	UINT16 ed;
	UINT8 gun_ready = (shoot_cooldown == 0);

	if (rt_room != current_room) {
		RT_BuildWalk(p);
		rt_scan_age = 200;
		e2e_errand = 0;
	}
	portal = E2E_FindType(NextLevelPortal);
	if (portal) {
		ox = portal->x;
		oy = portal->y;
	} else {
		ox = rt_portal_x[current_room < 5 ? current_room : 4];
		oy = rt_portal_y[current_room < 5 ? current_room : 4];
	}
	{
		/* Power-ups are worth the detour: electricity (room 1) and the
		 * fire-rate chip (map4's maze, 2.5x fire rate against 5+ HP enemies on
		 * later waves). While one is still lying in the room it is the goal
		 * instead of the portal: same farm-coins / buy-the-blocking-doors /
		 * walk-there loop a player would use. */
		Sprite* pu = E2E_FindType(ElectricityPickup);
		if (!pu) pu = E2E_FindType(FireRateChip);
		if (pu) {
			ox = pu->x;
			oy = pu->y;
			going_for_pickup = 1;
		}
	}

	if (rt_block >= 0) door = RT_DoorAt(rt_block_x, rt_block_y);
	if (rt_block >= 0 && !door) e2e_errand = 0; /* door opened: bank for the next one */
	if (++rt_scan_age >= 90 || (rt_block >= 0 && !door)) {
		rt_scan_age = 0;
		rt_coin_reach = 0xFF; /* doors may have opened: re-check pickups */
		RT_SnapshotDoors();
		if (RT_Plan(p, ox, oy, 4, -1, 1, &rt_block, &rt_route_cost) && rt_block >= 0) {
			rt_block_x = rt_door_x[rt_block];
			rt_block_y = rt_door_y[rt_block];
			door = RT_DoorAt(rt_block_x, rt_block_y);
		} else {
			rt_block = -1;
			door = 0;
		}
		rt_len = 0; /* the passable plan isn't walkable; force a real replan */
		rt_tx = 0xFFFF;
	}

	/* Remember the spawn point the newest enemy came out of. */
	if (enemies_left_to_spawn < rt_left_prev && rt_left_prev != 0xFF) {
		UINT8 i;
		Sprite* spr;
		Sprite* sp;
		Sprite* best = 0;
		UINT16 bd = 0xFFFF;
		SPRITEMANAGER_ITERATE(i, spr) {
			UINT8 j;
			if (!IsEnemyType(spr->type)) continue;
			SPRITEMANAGER_ITERATE(j, sp) {
				UINT16 d;
				if (sp->type != SpawnPoint) continue;
				d = E2E_Dist(spr, sp->x, sp->y);
				if (d < bd) { bd = d; best = sp; }
			}
		}
		if (best && bd < 24) {
			rt_hot.x = best->x;
			rt_hot.y = best->y;
			rt_hot.coll_w = 16;
			rt_hot.coll_h = 16;
			rt_have_hot = 1;
		}
	}
	rt_left_prev = enemies_left_to_spawn;
	e2e_t_cost = rt_route_cost;
	e2e_t_block = rt_block;
	e2e_t_errand = e2e_errand;
	e2e_t_doors = rt_ndoors;
	enemy = E2E_NearestEnemy(p, &ed);
	if (enemy) { e2e_ex = enemy->x; e2e_ey = enemy->y; e2e_et = enemy->type; } else { e2e_ex = 0xFFFF; e2e_ey = 0xFFFF; e2e_et = 0xFF; }
	e2e_skip_portal = going_for_pickup;

	if (!e2e_errand) {
		/* Bank the next door's cost (opening doors unlocks closer spawns,
		 * which speeds farming up), then leave when it's quiet. */
		UINT8 need = door ? door->custom_data[CD_DOOR_COST]
		                  : (going_for_pickup ? 0 : e2e_farm_reserve[current_room < 5 ? current_room : 4]);
		/* Doors and pickups often sit in pockets: only go when the wave is
		 * done (or nothing is alive and nothing is left to spawn). */
		if (ready_coins >= need && (E2E_WaveCleared() || (!enemy && enemies_left_to_spawn == 0))) {
			e2e_errand = 1;
		} else {
			if ((!enemy || ed > 64) && !e2e_escape_hold) {
				Sprite* coin = E2E_FindType(CoinsPickup);
				if (coin && (coin->x != rt_coin_x || coin->y != rt_coin_y || rt_coin_reach == 0xFF)) {
					/* New pickup: is it reachable without opening doors? */
					rt_coin_x = coin->x;
					rt_coin_y = coin->y;
					RT_SnapshotDoors();
					rt_coin_reach = RT_Plan(p, coin->x, coin->y, 4, -1, 0, 0, 0);
					if (rt_throttled) rt_coin_reach = 0xFF; /* not checked yet */
					rt_tx = 0xFFFF;
				}
				if (coin && rt_coin_reach == 1) { e2e_dbg = 1; return RT_Go(p, coin->x, coin->y, 4, 0); }
			}
			/* Nothing hittable from here (walls, enemy stuck in a wall):
			 * walk to a tile with a clear shot. A target that hasn't moved
			 * for 2 s (stuck in a wall) isn't chasing us, so only real
			 * contact danger stops that. */
			if (enemy) {
				/* "Still" = stays inside a small box (enemies stuck against a
				 * wall or closed door jitter by a pixel or two). */
				if (Abs16((INT16)enemy->x - (INT16)rt_still_x) <= 4 &&
				    Abs16((INT16)enemy->y - (INT16)rt_still_y) <= 4) {
					if (rt_still < 255) rt_still++;
				} else {
					rt_still = 0;
					rt_still_x = enemy->x;
					rt_still_y = enemy->y;
				}
			}
			if (enemy && !e2e_escape_hold && /* not threatened (walking distance): go get a shot */
			    E2E_DangerAt((UINT16)(p->x + (p->coll_w >> 1)), (UINT16)(p->y + (p->coll_h >> 1))) >=
			        (rt_still > 120 ? 14 : 48)) {
				UINT8 face;
				if (!E2E_Aligned(p, enemy, &face)) {
					UINT8 k = RT_GoFire(p, enemy);
					e2e_dbg = k ? 2 : 3;
					if (k) return k;
					/* No firing spot anywhere we can reach: lure it out. */
					k = RT_GoClosest(p, enemy);
					if (k) { e2e_dbg = 6; return k; }
				}
			}
			/* Nothing close: camp a firing lane on the spawn point enemies
			 * have been coming out of (>= 40 px away), instead of waiting
			 * for them to cross the whole maze. */
			if (rt_have_hot && (!enemy || ed > 96) && !e2e_escape_hold) {
				UINT8 face;
				if (!E2E_Aligned(p, &rt_hot, &face) ||
				    E2E_Dist(p, rt_hot.x, rt_hot.y) < 40) {
					UINT8 k;
					rt_fire_min = 40;
					k = RT_GoFire(p, &rt_hot);
					rt_fire_min = 24;
					if (k) { e2e_dbg = 4; return k; }
				} else {
					e2e_dbg = 5;
					return 0; /* in position: wait */
				}
			}
			e2e_dbg = (UINT8)(e2e_dbg | 0x10);
			return E2E_Fight(p, enemy, gun_ready);
		}
	}
	/* On the errand: if a door got pricier than our coins (shouldn't), farm. */
	if (door && ready_coins < door->custom_data[CD_DOOR_COST]) {
		e2e_errand = 0;
		return E2E_Fight(p, enemy, gun_ready);
	}
	/* An enemy came close (often one that just spawned near the door or
	 * pickup): drop the errand and fight it out in the open, come back later. */
	if (enemy && ed < 48) {
		e2e_errand = 0;
		return E2E_Fight(p, enemy, gun_ready);
	}

	/* Free shot if lined up and close; dodge real danger; else go. */
	if (gun_ready) {
		UINT8 i;
		UINT8 face;
		Sprite* spr;
		SPRITEMANAGER_ITERATE(i, spr) {
			if (!IsEnemyType(spr->type)) continue;
			if (E2E_Aligned(p, spr, &face) && E2E_Dist(p, spr->x, spr->y) < 48) return (UINT8)(face | J_B);
		}
	}
	e2e_danger_spawns = 0;
	if (e2e_escape_hold ||
	    E2E_DangerAt((UINT16)(p->x + (p->coll_w >> 1)), (UINT16)(p->y + (p->coll_h >> 1))) < 14) {
		UINT8 k = E2E_Flee(p, 0, 0);
		e2e_danger_spawns = 1;
		return k;
	}
	e2e_danger_spawns = 1;
	if (door) {
		e2e_door_target = door;
		return RT_Go(p, door->x, door->y, 0, door);
	}
	return RT_Go(p, ox, oy, 2, 0);
}
#endif /* E2E_USES_PLANNER */

/* ------------------------------------------------------------ hooks */

void E2E_OnMenuUpdate(void) BANKED {
	e2e_keys = 0;
	if (E2E_TickVerdict()) return;
	/* E2E_SEED: wait a few more frames on the title screen, like a player
	 * pressing START a moment later; this changes the game's randomness so
	 * one scenario can be checked across several different games. */
	if (e2e_phase == E2E_PHASE_BOOT && e2e_frames >= E2E_BOOT_FRAMES + (UINT32)E2E_SEED * 7UL) {
		/* Press START like a player; StateMenu handles it. */
		e2e_keys = J_START;
		e2e_phase = E2E_PHASE_PLAY;
		e2e_wait = 0;
	}
}

void E2E_OnGameUpdate(void) BANKED {
	Sprite* p = scroll_target;
	if (E2E_TickVerdict()) return;
	E2E_CacheHazards();
	E2E_TrackStuck(p);
	e2e_keys = 0;
	e2e_door_target = 0;
	e2e_updates++;
#if E2E_USES_PLANNER
	if (rt_plan_cd) rt_plan_cd--;
#endif
	if (p && current_room > e2e_max_room) e2e_max_room = current_room;
	if (p) { e2e_px = p->x; e2e_py = p->y; }
	e2e_dbg = 0;
	if (!p || pending_room_transition) return;

	if (e2e_phase == E2E_PHASE_PLAY) {
		if (++e2e_wait >= E2E_ACTION_DELAY) {
			e2e_coins_before = ready_coins;
			e2e_phase = E2E_PHASE_RUN;
		}
		return;
	}

#if E2E_SCENARIO == E2E_SCENARIO_DIE
	{
		/* Walk into enemies, never shoot, until a real Game Over. */
		Sprite* enemy = E2E_NearestEnemy(p, 0);
		if (enemy) e2e_keys = E2E_Walk(p, enemy->x, enemy->y, 0);
	}

#elif E2E_SCENARIO == E2E_SCENARIO_COINS
	{
		Sprite* enemy;
		if (ready_coins > e2e_coins_before) {
			DPRINT_POS(0, 0);
			DPrintf("    COINS OK      ");
			E2E_Finish(E2E_VERDICT_PASS);
			return;
		}
		enemy = E2E_NearestEnemy(p, 0);
		if (enemy) e2e_keys = E2E_Fight(p, enemy, shoot_cooldown == 0);
	}

#elif E2E_SCENARIO == E2E_SCENARIO_ROOM1_DIE
	if (current_room == 0) {
		e2e_keys = E2E_GoToPortal(p);
	} else {
		/* In room 1 for real: now walk into enemies until Game Over. */
		Sprite* enemy = E2E_NearestEnemy(p, 0);
		if (enemy) e2e_keys = E2E_Walk(p, enemy->x, enemy->y, 0);
	}

#elif E2E_SCENARIO == E2E_SCENARIO_ELECTRIC
	if (player_electric_attack) {
		if (current_room == 1) {
			DPRINT_POS(0, 0);
			DPrintf("   ELECTRIC OK    ");
			E2E_Finish(E2E_VERDICT_PASS);
		} else {
			EMU_MESSAGE("E2E FAIL electric outside room1");
			E2E_Finish(E2E_VERDICT_FAIL);
		}
		return;
	}
	if (current_room == 0) {
		/* Bank enough kill coins for BOTH doors (room0 + room1) before
		 * leaving, so room 1 (harder waves) is a straight run to the pickup. */
		UINT16 need = 10;
		Sprite* enemy = E2E_NearestEnemy(p, 0);
		if (E2E_ClosedDoorNear(p->x, p->y, 0xFFFF)) need += 10;
		/* Leave only once the coins are banked AND the wave is cleared:
		 * walk to the door + portal during the break between waves. */
		if (ready_coins >= need && enemies_left_to_spawn == 0 &&
		    enemies_killed == enemies_to_spawn) e2e_errand = 1; /* game's own "wave cleared" */
		if (!e2e_errand) {
			e2e_keys = E2E_Fight(p, enemy, shoot_cooldown == 0);
		} else {
			e2e_keys = E2E_GoToPortal(p);
		}
	} else if (current_room == 1) {
		Sprite* pickup = E2E_FindType(ElectricityPickup);
		if (pickup) {
			e2e_keys = E2E_RoomBot(p, pickup->x, pickup->y, 0);
		} else {
			EMU_MESSAGE("E2E FAIL no electricity pickup in room1");
			E2E_Finish(E2E_VERDICT_FAIL);
		}
	} else {
		EMU_MESSAGE("E2E FAIL passed room1 without electricity");
		E2E_Finish(E2E_VERDICT_FAIL);
	}

#elif E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR
	/* Whole game, rooms 0-4: farm coins, buy doors, grab every power-up
	 * (E2E_LevelBot picks the goals), take each portal. The boss run, boss
	 * fight and the Win screen are handled by the hooks below. */
	e2e_t_rapid = player_rapid_fire;
	e2e_keys = E2E_LevelBot(p);
#endif

#if E2E_SCENARIO != E2E_SCENARIO_DIE
	if (!e2e_verdict) e2e_keys = E2E_AvoidDoors(p, e2e_keys);
#endif
}

#if E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR
UINT8 E2E_BossRunBot(Sprite* p) BANKED; /* E2EBoss.c (its own ROM bank) */
#endif

void E2E_OnBossUpdate(void) BANKED {
	Sprite* p = boss_run_player;
	if (E2E_TickVerdict()) return;
	E2E_CacheHazards();
	E2E_TrackStuck(p);
	e2e_keys = 0;
#if E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR
	/* Natural CameraDriver scroll only. Keep toward the right side of the
	 * screen and wall-follow vertically through gaps. */
	if (p) {
		e2e_px = p->x;
		e2e_py = p->y;
		e2e_ex = (UINT16)scroll_x;
		e2e_keys = E2E_BossRunBot(p);
	}
#endif
}

void E2E_OnBossFightUpdate(void) BANKED {
	Sprite* p = boss_fight_player;
	if (E2E_TickVerdict()) return;
	E2E_CacheHazards();
	E2E_TrackStuck(p);
	e2e_keys = 0;
#if E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR
	if (p) {
		Sprite* boss = E2E_FindType(Boss);
		if (boss) e2e_keys = E2E_Fight(p, boss, boss_fight_shoot_cooldown == 0);
	}
#endif
}

void E2E_OnGameOverStart(void) BANKED {
	e2e_keys = 0;
#if E2E_SCENARIO == E2E_SCENARIO_DIE
	E2E_Finish(E2E_VERDICT_PASS);
#elif E2E_SCENARIO == E2E_SCENARIO_ROOM1_DIE
	if (e2e_max_room >= 1) {
		E2E_Finish(E2E_VERDICT_PASS);
	} else {
		EMU_MESSAGE("E2E FAIL died before reaching room1");
		E2E_Finish(E2E_VERDICT_FAIL);
	}
#else
	EMU_MESSAGE("E2E FAIL unexpected GameOver");
	E2E_Finish(E2E_VERDICT_FAIL);
#endif
}

void E2E_OnGameOverUpdate(void) BANKED {
	E2E_TickVerdict();
}

void E2E_OnWinStart(void) BANKED {
	e2e_keys = 0;
#if E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR
	E2E_Finish(E2E_VERDICT_PASS);
#else
	EMU_MESSAGE("E2E FAIL unexpected Win");
	E2E_Finish(E2E_VERDICT_FAIL);
#endif
}

void E2E_OnWinUpdate(void) BANKED {
	E2E_TickVerdict();
}

#else /* !E2E_ENABLED */

void E2E_OnMenuUpdate(void) BANKED {}
void E2E_OnGameUpdate(void) BANKED {}
void E2E_OnBossUpdate(void) BANKED {}
void E2E_OnBossFightUpdate(void) BANKED {}
void E2E_OnGameOverStart(void) BANKED {}
void E2E_OnGameOverUpdate(void) BANKED {}
void E2E_OnWinStart(void) BANKED {}
void E2E_OnWinUpdate(void) BANKED {}

#endif
