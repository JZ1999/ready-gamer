#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "Scroll.h"
#include "Rooms.h"
#include "StateGame.h"
#include "FlowField.h"
#include "main.h"
#include <string.h>

/*
 * Maze-aware enemy chasing + stuck-enemy recovery (room game only).
 *
 * Flow field: one byte per map tile = walking distance (in tiles) from the
 * player for a 16x16 enemy whose top-left sits on that tile. Enemies step to
 * the neighbouring tile with the smallest distance, so they follow corridors
 * around walls instead of pushing straight at the player through them.
 *
 * Cost control (the DMG has ~70k cycles/frame): the field is relaxed in place
 * one map row every 4th frame, alternating forward/backward sweeps, so it
 * never spikes a frame (measured with the e2e runner: ~7% slowdown). Each
 * cell re-derives its value from its neighbours every pass, so it also
 * re-converges when the player moves (no reset needed).
 *
 * Walkable mask: rebuilt on room entry and whenever the number of closed
 * doors changes (closed doors are walls for enemies, see EnemyMove.c).
 *
 * Unstick: about once a second, an enemy that isn't getting closer to the
 * player (path distance not improving, or no path at all) gets a strike; at
 * 4 strikes (~4 s) it is moved to an unlocked spawn point away from the
 * player, so a jammed or trapped enemy can never stall a wave.
 */

#define FF_MAXW 50
#define FF_MAXH 18
#define FF_CELLS (FF_MAXW * FF_MAXH)
#define FF_ROWS_PER_FRAME 1
#define FF_FAR 255   /* not reached (yet) */
#define FF_WALL 254  /* not walkable: wall or closed door (never relaxed) */

#define TILE_SOLID(t) ((t) >= 1 && (t) <= 3) /* full + partial bricks */

static UINT8 ff_dist[FF_CELLS]; /* distance, or FF_WALL / FF_FAR */
static UINT8 ff_w;
static UINT8 ff_h;
static UINT8 ff_room = 0xFF;
static UINT8 ff_doors = 0xFF;
static UINT8 ff_row;
static UINT8 ff_backward;
static UINT8 ff_ready;
static UINT8 ff_tick;
static UINT8 ff_phase;

/* unstick bookkeeping, per sprite-pool slot */
static UINT16 us_uid[N_SPRITE_MANAGER_SPRITES];
static UINT8 us_strikes[N_SPRITE_MANAGER_SPRITES];
static UINT8 us_best[N_SPRITE_MANAGER_SPRITES]; /* best path distance seen */


static UINT8 CountClosedDoors(void) {
	UINT8 i;
	UINT8 n = 0;
	Sprite* spr;
	SPRITEMANAGER_ITERATE(i, spr) {
		if (spr->type == Door && spr->custom_data[CD_DOOR_STATE] == 0) n++;
	}
	return n;
}

static void BuildMask(void) {
	UINT8 cx;
	UINT8 cy;
	UINT16 idx = 0;
	UINT8 i;
	Sprite* spr;

	ff_w = (UINT8)(scroll_w >> 3);
	ff_h = (UINT8)(scroll_h >> 3);
	if (ff_w > FF_MAXW) ff_w = FF_MAXW;
	if (ff_h > FF_MAXH) ff_h = FF_MAXH;
	memset(ff_dist, FF_WALL, sizeof(ff_dist));
	for (cy = 0; cy + 1 < ff_h; ++cy) {
		idx = (UINT16)cy * ff_w;
		for (cx = 0; cx + 1 < ff_w; ++cx, ++idx) {
			if (TILE_SOLID(GetRoomTileFromTable(cx, cy)) || TILE_SOLID(GetRoomTileFromTable(cx + 1, cy)) ||
			    TILE_SOLID(GetRoomTileFromTable(cx, cy + 1)) || TILE_SOLID(GetRoomTileFromTable(cx + 1, cy + 1)))
				continue;
			ff_dist[idx] = FF_FAR;
		}
	}
	/* Closed doors: clear every cell where a 16x16 enemy would overlap one. */
	SPRITEMANAGER_ITERATE(i, spr) {
		INT16 x0;
		INT16 y0;
		INT16 x1;
		INT16 y1;
		INT16 c;
		INT16 r;
		if (spr->type != Door || spr->custom_data[CD_DOOR_STATE] != 0) continue;
		x0 = ((INT16)spr->x - 15) >> 3;
		y0 = ((INT16)spr->y - 15) >> 3;
		x1 = ((INT16)spr->x + spr->coll_w - 1) >> 3;
		y1 = ((INT16)spr->y + spr->coll_h - 1) >> 3;
		for (r = y0; r <= y1; ++r) {
			if (r < 0 || r >= ff_h) continue;
			for (c = x0; c <= x1; ++c) {
				UINT16 k;
				if (c < 0 || c >= ff_w) continue;
				k = (UINT16)r * ff_w + (UINT16)c;
				ff_dist[k] = FF_WALL;
			}
		}
	}
	ff_row = 0;
	ff_backward = 0;
	ff_ready = 1;
}

/* One row of in-place relaxation: dist = 1 + min(neighbours).
 * Written for SDCC/SM83: three moving pointers (this row, above, below),
 * the left neighbour carried in a local, and a constant all-wall row for the
 * map edges, so the loop has no index math and no edge tests except the last
 * column. */
static const UINT8 ff_wall_row[FF_MAXW] = {
	FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL,
	FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL,
	FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL,
	FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL,
	FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL, FF_WALL
};

static void RelaxRow(UINT8 cy, UINT8 pcx, UINT8 pcy) {
	UINT8 w = ff_w;
	UINT8* p = &ff_dist[(UINT16)cy * w];
	const UINT8* u = cy ? p - w : ff_wall_row;
	const UINT8* n = ((UINT8)(cy + 1) < ff_h) ? p + w : ff_wall_row;
	UINT8 left = FF_WALL;
	UINT8 cx = w;
	UINT8 target = (cy == pcy) ? (UINT8)(w - pcx) : 0; /* countdown value at the player's cell */
	UINT8 b;
	UINT8 v;
	while (cx) {
		v = *p;
		if (v != FF_WALL) {
			if (cx == target) {
				v = 0;
			} else {
				b = left;
				if (cx != 1 && p[1] < b) b = p[1];
				if (*u < b) b = *u;
				if (*n < b) b = *n;
				v = (b >= FF_WALL - 1) ? FF_FAR : (UINT8)(b + 1);
			}
			*p = v;
		}
		left = v;
		++p;
		++u;
		++n;
		--cx;
	}
}

static UINT8 DistAt(Sprite* e) {
	UINT8 cx = (UINT8)((e->x + 4) >> 3);
	UINT8 cy = (UINT8)((e->y + 4) >> 3);
	if (!ff_ready || cx >= ff_w || cy >= ff_h) return FF_FAR;
	return ff_dist[(UINT16)cy * ff_w + cx];
}

UINT8 FlowFieldDistTo(Sprite* e) BANKED {
	if (ff_room != current_room) return FF_FAR;
	return DistAt(e);
}

/*
 * "Stuck" = not getting any closer to the player: checked about once a
 * second, an enemy gets a strike when its path distance to the player hasn't
 * improved (or there is no path from where it is). Pinned against a wall,
 * jittering in a dead end, or trapped where it can't reach the player all
 * count. Enemies already close (distance <= 3 tiles) never get strikes. At 4
 * strikes it is moved to an unlocked spawn point away from the player.
 */
static void Unstick(void) {
	UINT8 i;
	Sprite* spr;
	SPRITEMANAGER_ITERATE(i, spr) {
		UINT8 d;
		if (!IsEnemyType(spr->type)) continue;
		d = DistAt(spr);
		if (us_uid[i] != spr->unique_id) {
			us_uid[i] = spr->unique_id;
			us_best[i] = d;
			us_strikes[i] = 0;
			continue;
		}
		if (d <= 3) {
			us_best[i] = d;
			us_strikes[i] = 0;
			continue;
		}
		if (d < FF_WALL && d < us_best[i]) {
			us_best[i] = d;
			us_strikes[i] = 0;
			continue;
		}
		if (++us_strikes[i] >= 4) {
			UINT16 x;
			UINT16 y;
			UINT16 bx = 0xFFFF;
			UINT16 by = 0;
			UINT8 tries;
			/* Only spawn points the enemy can WALK to the player from: a
			 * door can unlock a spawner that sits behind another closed door
			 * (map4: P2 unlocks S4 behind P3). Teleporting there trapped the
			 * enemy for good and the wave could never be finished. */
			for (tries = 0; tries != 8; ++tries) {
				INT16 ddx;
				INT16 ddy;
				UINT8 tx;
				UINT8 ty;
				GetRandomSpawnPositionFromTable(&x, &y);
				tx = (UINT8)((x + 4) >> 3);
				ty = (UINT8)((y + 4) >> 3);
				if (tx >= ff_w || ty >= ff_h || ff_dist[(UINT16)ty * ff_w + tx] >= FF_WALL) continue;
				bx = x;
				by = y;
				if (!scroll_target) break;
				ddx = (INT16)x - (INT16)scroll_target->x;
				ddy = (INT16)y - (INT16)scroll_target->y;
				if (ddx < 0) ddx = -ddx;
				if (ddy < 0) ddy = -ddy;
				if (ddx + ddy > 48) break;
			}
			if (bx == 0xFFFF) {
				/* No unlocked spawner can reach the player. map4: doors REPLACE
				 * the active spawners, so after P1 then P2 the only one left is
				 * S4, behind the still-closed P3 - every enemy appeared where it
				 * could never come out and the wave (and the coins to buy P3)
				 * never finished. Use any reachable spawner of the room. */
				UINT8 j;
				UINT16 far_best = 0;
				for (j = 0; GetRoomSpawnPointAny(j, &x, &y); ++j) {
					UINT8 tx = (UINT8)((x + 4) >> 3);
					UINT8 ty = (UINT8)((y + 4) >> 3);
					UINT16 dd = 1;
					if (tx >= ff_w || ty >= ff_h || ff_dist[(UINT16)ty * ff_w + tx] >= FF_WALL) continue;
					if (scroll_target) {
						INT16 ddx = (INT16)x - (INT16)scroll_target->x;
						INT16 ddy = (INT16)y - (INT16)scroll_target->y;
						if (ddx < 0) ddx = -ddx;
						if (ddy < 0) ddy = -ddy;
						dd = (UINT16)(ddx + ddy);
					}
					if (dd > far_best) { far_best = dd; bx = x; by = y; } /* farthest: no surprise spawn on the player */
				}
			}
			if (bx != 0xFFFF) {
				spr->x = bx;
				spr->y = by;
			}
			us_best[i] = FF_FAR;
			us_strikes[i] = 0;
		}
	}
}

void FlowFieldUpdate(void) BANKED {
	UINT8 n;
	UINT8 pcx = 0xFF;
	UINT8 pcy = 0xFF;
	UINT8 doors;

	if (!scroll_target) return;
	if (ff_room != current_room) {
		ff_room = current_room;
		ff_doors = CountClosedDoors();
		BuildMask();
		memset(us_uid, 0, sizeof(us_uid));
	}
	if ((++ff_tick & 31) == 0) {
		doors = CountClosedDoors();
		if (doors != ff_doors) {
			ff_doors = doors;
			BuildMask();
		}
	}
	if ((ff_tick & 63) == 0) Unstick();

	{
		UINT16 px = (scroll_target->x + 4) >> 3;
		UINT16 py = (scroll_target->y + 4) >> 3;
		if (px < ff_w && py < ff_h) {
			pcx = (UINT8)px;
			pcy = (UINT8)py;
		}
	}
	/* One row every 4th frame: a row costs ~0.5 frame of CPU with SDCC, so
	 * this keeps the game near full speed; a forward+backward refresh takes
	 * ~2.5 s, plenty for enemies tracking a walking player. */
	if (++ff_phase < 4) return;
	ff_phase = 0;
	for (n = 0; n != FF_ROWS_PER_FRAME; ++n) {
		if (!ff_backward) {
			RelaxRow(ff_row, pcx, pcy);
			if (++ff_row >= ff_h) {
				ff_row = (UINT8)(ff_h - 1);
				ff_backward = 1;
			}
		} else {
			RelaxRow(ff_row, pcx, pcy);
			if (ff_row == 0) {
				ff_backward = 0;
			} else {
				ff_row--;
			}
		}
	}
}

void EnemySteer(Sprite* e, INT8* dx, INT8* dy) BANKED {
	UINT8 cx;
	UINT8 cy;
	UINT16 idx;
	UINT8 here;
	UINT8 best;
	INT8 bdx = 0;
	INT8 bdy = 0;

	if (!ff_ready || ff_room != current_room || current_state != StateGame) return;
	cx = (UINT8)((e->x + 4) >> 3);
	cy = (UINT8)((e->y + 4) >> 3);
	if (cx >= ff_w || cy >= ff_h) return;
	idx = (UINT16)cy * ff_w + cx;
	here = ff_dist[idx];
	if (here >= FF_WALL) {
		/* Off the grid (e.g. pushed half a tile into a wall corner by the
		 * plain chase): slide onto the best walkable tile it overlaps, or the
		 * enemy jams against the wall forever. */
		/* Candidate top-left tiles within one tile of where it stands. */
		UINT8 x0 = (UINT8)(cx ? cx - 1 : 0);
		UINT8 y0 = (UINT8)(cy ? cy - 1 : 0);
		UINT8 ox;
		UINT8 oy;
		UINT8 bx = 0xFF;
		UINT8 by = 0;
		best = FF_WALL;
		for (oy = 0; oy != 3; ++oy) {
			for (ox = 0; ox != 3; ++ox) {
				UINT8 tx = (UINT8)(x0 + ox);
				UINT8 ty = (UINT8)(y0 + oy);
				UINT8 d;
				if (tx >= ff_w || ty >= ff_h) continue;
				d = ff_dist[(UINT16)ty * ff_w + tx];
				if (d < best) { best = d; bx = tx; by = ty; }
			}
		}
		if (bx == 0xFF || best <= 2) return;
		{
			UINT16 tx = (UINT16)bx << 3;
			UINT16 ty = (UINT16)by << 3;
			*dx = (e->x < tx) ? 1 : (e->x > tx) ? -1 : 0;
			*dy = (e->y < ty) ? 1 : (e->y > ty) ? -1 : 0;
			if (*dx && *dy) *dy = 0; /* one axis at a time */
		}
		return;
	}
	if (here <= 2) return; /* close: plain chase */
	best = here;
	if (cx && ff_dist[idx - 1] < best) { best = ff_dist[idx - 1]; bdx = -1; bdy = 0; }
	if (cx + 1 < ff_w && ff_dist[idx + 1] < best) { best = ff_dist[idx + 1]; bdx = 1; bdy = 0; }
	if (cy && ff_dist[idx - ff_w] < best) { best = ff_dist[idx - ff_w]; bdx = 0; bdy = -1; }
	if (cy + 1 < ff_h && ff_dist[idx + ff_w] < best) { best = ff_dist[idx + ff_w]; bdx = 0; bdy = 1; }
	if (!bdx && !bdy) return;
	/* Move along the corridor, and slide the other axis onto the tile grid
	 * so a 16x16 body lines up with 2-tile-wide corridors. */
	if (bdx) {
		UINT16 ty = (UINT16)cy << 3;
		*dx = bdx;
		*dy = (e->y < ty) ? 1 : (e->y > ty) ? -1 : 0;
	} else {
		UINT16 tx = (UINT16)cx << 3;
		*dy = bdy;
		*dx = (e->x < tx) ? 1 : (e->x > tx) ? -1 : 0;
	}
}
