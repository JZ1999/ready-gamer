#include "Banks/SetAutoBank.h"

#include "E2E.h"

#if E2E_ENABLED && E2E_USES_PLANNER

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "StateGame.h"
#include "Rooms.h"
#include "Scroll.h"
#include "E2EPlan.h"
#include "main.h"

#include <string.h>

/* Split out of E2E.c so the autopilot fits in two 16 KB ROM banks. */

extern UINT16 ready_coins;
extern UINT8 current_room;

static INT16 Abs16(INT16 v) { return v < 0 ? (INT16)(-v) : v; }

/* ------------------------------------------------------------ whole-map route planner
 *
 * Maze rooms (map4/map5) are 40-50 tiles wide, so the screen-sized planner
 * above can't see the route. Here the whole room is one 8 px grid aligned to
 * map tiles. On room entry we probe every tile-to-tile step ONCE with a ghost
 * hitbox through SafeTranslateSprite (the player's own wall test; it only
 * reads the map) and cache it as two bits per tile. After that a BFS is just
 * array lookups. Closed doors are handled per plan:
 *   - "passable" plans ignore doors: used to find WHICH doors stand between
 *     us and the goal (and how many coins that route costs),
 *   - normal plans treat doors as walls and keep out of the 17 px auto-buy
 *     range of any door we can afford but don't mean to buy.
 */
#define RT_CELLS (RT_MAXW * RT_MAXH)

UINT8 rt_walk[RT_CELLS]; /* bit0: can step right, bit1: can step down */
static UINT8 rt_from[RT_CELLS]; /* 0 unseen, 1..4 came from dir, 5 start, 6 door-blocked */
UINT16 rt_row[RT_MAXH];  /* row start index (no multiplies in the BFS) */
static UINT8 rt_solid[(RT_CELLS + 7) / 8]; /* 1 bit per tile: wall (full or partial brick) */
static UINT8 rt_qx[256];
static UINT8 rt_qy[256];
UINT8 rt_px[RT_PATH_MAX];
UINT8 rt_py[RT_PATH_MAX];
UINT8 rt_len;
UINT8 rt_pos;
UINT8 rt_w;
UINT8 rt_h;
UINT8 rt_room = 0xFF;
UINT16 rt_tx = 0xFFFF;
UINT16 rt_ty = 0xFFFF;
UINT16 rt_age;
static const INT8 rt_dx[4] = { 0, 0, -1, 1 };
static const INT8 rt_dy[4] = { -1, 1, 0, 0 };

/* Closed doors snapshot for one plan (doors are few; sprite pointers are
 * NOT kept across frames because removed sprites' slots get reused). */
UINT8 rt_ndoors;
UINT16 rt_door_x[RT_MAX_DOORS];
UINT16 rt_door_y[RT_MAX_DOORS];
static UINT8 rt_door_w[RT_MAX_DOORS];
static UINT8 rt_door_h[RT_MAX_DOORS];
static UINT8 rt_door_cost[RT_MAX_DOORS];

void RT_SnapshotDoors(void) BANKED {
	UINT8 i;
	Sprite* spr;
	rt_ndoors = 0;
	SPRITEMANAGER_ITERATE(i, spr) {
		if (!IsDoorType(spr->type) || spr->custom_data[CD_DOOR_STATE] != 0) continue;
		if (rt_ndoors == RT_MAX_DOORS) break;
		rt_door_x[rt_ndoors] = spr->x;
		rt_door_y[rt_ndoors] = spr->y;
		rt_door_w[rt_ndoors] = spr->coll_w;
		rt_door_h[rt_ndoors] = spr->coll_h;
		rt_door_cost[rt_ndoors] = spr->custom_data[CD_DOOR_COST];
		rt_ndoors++;
	}
}

/* Is a player box on tile (tx, ty) inside a closed door (last snapshot)? */
UINT8 RT_DoorTile(UINT8 tx, UINT8 ty) BANKED {
	UINT8 i;
	UINT16 x = (UINT16)tx << 3;
	UINT16 y = (UINT16)ty << 3;
	for (i = 0; i != rt_ndoors; ++i) {
		INT16 dx = (INT16)rt_door_x[i] - (INT16)x;
		INT16 dy = (INT16)rt_door_y[i] - (INT16)y;
		if (dx < 13 && dx + (INT16)rt_door_w[i] > 3 && dy < 13 && dy + (INT16)rt_door_h[i] > 3) return 1;
	}
	return 0;
}

Sprite* RT_DoorAt(UINT16 x, UINT16 y) BANKED {
	UINT8 i;
	Sprite* spr;
	SPRITEMANAGER_ITERATE(i, spr) {
		if (IsDoorType(spr->type) && spr->custom_data[CD_DOOR_STATE] == 0 && spr->x == x && spr->y == y)
			return spr;
	}
	return 0;
}

void RT_BuildWalk(Sprite* p) BANKED {
	Sprite g;
	UINT8 cx;
	UINT8 cy;
	UINT16 idx = 0;
	rt_w = (UINT8)(scroll_w >> 3);
	rt_h = (UINT8)(scroll_h >> 3);
	if (rt_w > RT_MAXW) rt_w = RT_MAXW;
	if (rt_h > RT_MAXH) rt_h = RT_MAXH;
	g.coll_w = p->coll_w;
	g.coll_h = p->coll_h;
	for (cy = 0; cy != rt_h; ++cy) rt_row[cy] = (UINT16)cy * rt_w;
	memset(rt_solid, 0, sizeof(rt_solid));
	for (cy = 0; cy != rt_h; ++cy) {
		for (cx = 0; cx != rt_w; ++cx) {
			UINT8 t = GetRoomTileFromTable(cx, cy);
			if (t >= 1 && t <= 3) {
				UINT16 i2 = rt_row[cy] + cx;
				rt_solid[i2 >> 3] |= (UINT8)(1 << (i2 & 7));
			}
		}
	}
	for (cy = 0; cy != rt_h; ++cy) {
		for (cx = 0; cx != rt_w; ++cx, ++idx) {
			UINT8 v = 0;
			/* An edge is walkable only if BOTH directions are: the wall test
			 * only checks the leading edge, so a ghost inside a wall can
			 * "step out" of it, which used to let routes walk into walls. */
			if (cx + 1 < rt_w) {
				g.x = (UINT16)cx << 3;
				g.y = (UINT16)cy << 3;
				if (!SafeTranslateSprite(&g, 4, 0) && !SafeTranslateSprite(&g, 4, 0) &&
				    !SafeTranslateSprite(&g, -4, 0) && !SafeTranslateSprite(&g, -4, 0)) v |= 1;
			}
			if (cy + 1 < rt_h) {
				g.x = (UINT16)cx << 3;
				g.y = (UINT16)cy << 3;
				if (!SafeTranslateSprite(&g, 0, 4) && !SafeTranslateSprite(&g, 0, 4) &&
				    !SafeTranslateSprite(&g, 0, -4) && !SafeTranslateSprite(&g, 0, -4)) v |= 2;
			}
			rt_walk[idx] = v;
		}
	}
	rt_room = current_room;
	rt_tx = 0xFFFF;
	rt_len = 0;
}

static UINT8 RT_Solid(UINT8 c, UINT8 r) {
	UINT16 i;
	if (c >= rt_w || r >= rt_h) return 1;
	i = rt_row[r] + c;
	return rt_solid[i >> 3] & (UINT8)(1 << (i & 7));
}

/*
 * Same answer as the shot simulation in E2E_LOSFrom, read off the tile
 * bitmap. A shot is an 8x8 box whose wall test uses a 3 px inset (2x2 core),
 * so it flies along the 1-2 tile rows/columns under that core and stops when
 * the core's leading edge enters a wall tile. Partial bricks count as walls
 * (slightly pessimistic). Range 88 px like the simulation. *ok = 0 when the
 * bitmap isn't built for this room (caller falls back to simulating).
 */
UINT8 RT_LOSFast(UINT16 x, UINT16 y, Sprite* e, UINT8* ok) BANKED {
	INT16 ex0 = (INT16)e->x;
	INT16 ey0 = (INT16)e->y;
	INT16 ex1 = (INT16)(e->x + e->coll_w);
	INT16 ey1 = (INT16)(e->y + e->coll_h);
	INT16 sx = (INT16)x;
	INT16 sy = (INT16)y;
	UINT8 a0;
	UINT8 a1;
	INT16 c;
	*ok = 0;
	if (rt_room != current_room || !rt_w || current_state != StateGame) return 0;
	*ok = 1;
	if (sx + 8 > ex0 && sx < ex1 && sy + 8 > ey0 && sy < ey1) return (ex0 + ex1 > sx + sx + 8) ? J_RIGHT : J_LEFT; /* touching */
	/* row lane: left / right */
	if (sy + 8 > ey0 && sy < ey1) {
		a0 = (UINT8)((sy + 3) >> 3);
		a1 = (UINT8)((sy + 4) >> 3);
		if (ex1 > sx) { /* right */
			if (ex0 < sx + 96) {
				for (c = (sx + 4) >> 3; c < (INT16)rt_w; ++c) {
					if (c > ((sx + 4) >> 3) && (RT_Solid((UINT8)c, a0) || RT_Solid((UINT8)c, a1))) break;
					if (ex0 < (c << 3) + 11) return J_RIGHT; /* box reaches it before this wall */
				}
			}
		}
		if (ex0 < sx) { /* left */
			if (ex1 > sx - 88) {
				for (c = (sx + 3) >> 3; c >= 0; --c) {
					if (c < ((sx + 3) >> 3) && (RT_Solid((UINT8)c, a0) || RT_Solid((UINT8)c, a1))) break;
					if (ex1 > (c << 3) - 3) return J_LEFT;
				}
			}
		}
	}
	/* column lane: up / down */
	if (sx + 8 > ex0 && sx < ex1) {
		a0 = (UINT8)((sx + 3) >> 3);
		a1 = (UINT8)((sx + 4) >> 3);
		if (ey1 > sy) { /* down */
			if (ey0 < sy + 96) {
				for (c = (sy + 4) >> 3; c < (INT16)rt_h; ++c) {
					if (c > ((sy + 4) >> 3) && (RT_Solid(a0, (UINT8)c) || RT_Solid(a1, (UINT8)c))) break;
					if (ey0 < (c << 3) + 11) return J_DOWN;
				}
			}
		}
		if (ey0 < sy) { /* up */
			if (ey1 > sy - 88) {
				for (c = (sy + 3) >> 3; c >= 0; --c) {
					if (c < ((sy + 3) >> 3) && (RT_Solid(a0, (UINT8)c) || RT_Solid(a1, (UINT8)c))) break;
					if (ey1 > (c << 3) - 3) return J_UP;
				}
			}
		}
	}
	return 0;
}

/* Is the player box at pixel (x, y) blocked by a closed door, or inside the
 * auto-buy range of an affordable door that isn't `want_i`? */
static UINT8 RT_DoorBlocked(UINT16 x, UINT16 y, INT8 want_i) {
	UINT8 i;
	for (i = 0; i != rt_ndoors; ++i) {
		INT16 dx = (INT16)rt_door_x[i] - (INT16)x;
		INT16 dy = (INT16)rt_door_y[i] - (INT16)y;
		/* Door body overlaps our (inset) 16x16 box -> wall. */
		if (dx < 13 && dx + (INT16)rt_door_w[i] > 3 && dy < 13 && dy + (INT16)rt_door_h[i] > 3) return 1;
		if ((INT8)i != want_i && ready_coins >= rt_door_cost[i] && Abs16(dx) <= 18 && Abs16(dy) <= 18) return 1;
	}
	return 0;
}

/*
 * BFS from the player's tile to the goal: within r px of (tx, ty), or (if
 * want_i >= 0) inside door want_i's buy range. passable=1 ignores doors.
 * Fills rt_px/rt_py with the first RT_PATH_MAX tiles of the route.
 * Returns 1 if found. If door_on_route != 0 it receives the index of the
 * first closed door the route passes through (passable plans), else -1, and
 * route_cost the total cost of all doors on the route.
 */
/* Search budget: the bot's route/fire-spot/lure behaviours share one route
 * buffer and used to invalidate each other's routes, re-running a whole-map
 * search almost every frame (the game dropped to ~1 update per 5-50 frames
 * in map4). Non-passable searches now run at most once per RT_PLAN_GAP
 * frames; in between, callers fall back to a greedy step. */
#define RT_PLAN_GAP 20
UINT8 rt_plan_cd;
UINT8 rt_throttled; /* last RT_Plan returned 0 because of the budget */
Sprite* rt_los_target; /* want_i == -2: goal = a tile with a clear shot */
UINT8 rt_fire_min = 24;  /* ...at least this far (manhattan px) from it */

/* Tiles t with lo <= t*8 <= hi, as [*a, *b] (empty when *a > *b). */
static void RT_TileRange(INT16 lo, INT16 hi, UINT8* a, UINT8* b) {
	*a = lo <= 0 ? 0 : (lo >= 8 * 255 ? 255 : (UINT8)((lo + 7) >> 3));
	if (hi < 0) { *a = 1; *b = 0; return; }
	*b = hi >= 8 * 254 ? 254 : (UINT8)(hi >> 3);
}

UINT8 RT_Plan(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, INT8 want_i, UINT8 passable,
                     INT8* door_on_route, UINT8* route_cost) BANKED {
	UINT8 head = 0;
	UINT8 tail = 0;
	UINT8 sx;
	UINT8 sy;
	UINT8 cx = 0;
	UINT8 cy = 0;
	UINT8 found = 0;
	UINT8 d;
	UINT8 len;
	UINT8 i;
	UINT8 seen = 0;
	UINT8 best_d = 0xFF;
	UINT8 los_ok;
	UINT8 bx = 0;
	UINT8 by = 0;

	/* Goal tests in tile units (no 16-bit math per BFS cell):
	 *  -1 / door: goal rectangle [gx0..gx1] x [gy0..gy1]
	 *  -2: lane columns [gx0..gx1] or lane rows [gy0..gy1], then real LOS
	 *  -3: tile manhattan to (tcx, tcy) */
	UINT8 gx0 = 1, gx1 = 0, gy0 = 1, gy1 = 0;
	UINT8 tcx = (UINT8)(tx >> 3);
	UINT8 tcy = (UINT8)(ty >> 3);

	if (want_i == -2) {
		RT_TileRange((INT16)rt_los_target->x - 7, (INT16)(rt_los_target->x + rt_los_target->coll_w) - 1, &gx0, &gx1);
		RT_TileRange((INT16)rt_los_target->y - 7, (INT16)(rt_los_target->y + rt_los_target->coll_h) - 1, &gy0, &gy1);
	} else if (want_i >= 0) {
		RT_TileRange((INT16)rt_door_x[want_i] - 16, (INT16)rt_door_x[want_i] + 16, &gx0, &gx1);
		RT_TileRange((INT16)rt_door_y[want_i] - 16, (INT16)rt_door_y[want_i] + 16, &gy0, &gy1);
	} else if (want_i == -1) {
		RT_TileRange((INT16)tx - r, (INT16)tx + r, &gx0, &gx1);
		RT_TileRange((INT16)ty - r, (INT16)ty + r, &gy0, &gy1);
	}
	rt_throttled = 0;
	if (!passable) {
		if (rt_plan_cd) {
			rt_throttled = 1;
			if (door_on_route) *door_on_route = -1;
			if (route_cost) *route_cost = 0;
			return 0; /* rt_len/rt_pos untouched: keep following the old route */
		}
		rt_plan_cd = RT_PLAN_GAP;
	}

	sx = (UINT8)((p->x + 4) >> 3);
	sy = (UINT8)((p->y + 4) >> 3);
	if (sx >= rt_w) sx = (UINT8)(rt_w - 1);
	if (sy >= rt_h) sy = (UINT8)(rt_h - 1);
	memset(rt_from, 0, sizeof(rt_from));
	/* Door cells are marked once up front (6 = blocked) instead of testing
	 * every door on every BFS step: same rule as RT_DoorBlocked. */
	if (!passable) {
		for (i = 0; i != rt_ndoors; ++i) {
			UINT8 c0 = (UINT8)(rt_door_x[i] >> 3);
			UINT8 r0 = (UINT8)(rt_door_y[i] >> 3);
			UINT8 bx2;
			UINT8 by2;
			c0 = c0 > 3 ? (UINT8)(c0 - 3) : 0;
			r0 = r0 > 3 ? (UINT8)(r0 - 3) : 0;
			for (by2 = r0; by2 != (UINT8)(r0 + 8) && by2 < rt_h; ++by2)
				for (bx2 = c0; bx2 != (UINT8)(c0 + 8) && bx2 < rt_w; ++bx2)
				{
					INT16 dx = (INT16)rt_door_x[i] - (INT16)((UINT16)bx2 << 3);
					INT16 dy = (INT16)rt_door_y[i] - (INT16)((UINT16)by2 << 3);
					/* same rule as RT_DoorBlocked, for door i only */
					if ((dx < 13 && dx + (INT16)rt_door_w[i] > 3 && dy < 13 && dy + (INT16)rt_door_h[i] > 3) ||
					    ((INT8)i != want_i && ready_coins >= rt_door_cost[i] && Abs16(dx) <= 18 && Abs16(dy) <= 18))
						rt_from[rt_row[by2] + bx2] = 6;
				}
		}
	}
	rt_from[rt_row[sy] + sx] = 5;
	rt_qx[tail] = sx;
	rt_qy[tail] = sy;
	tail++;

	while (head != tail) {
		UINT16 idx;
		UINT16 nidx;
		cx = rt_qx[head];
		cy = rt_qy[head];
		head++;
		if (want_i == -3) {
			UINT8 dd = (UINT8)((cx > tcx ? cx - tcx : tcx - cx) + (cy > tcy ? cy - tcy : tcy - cy));
			if (dd < best_d) { best_d = dd; bx = cx; by = cy; }
		} else if (want_i == -2) {
			if ((cx >= gx0 && cx <= gx1) || (cy >= gy0 && cy <= gy1)) {
				UINT16 x = (UINT16)cx << 3;
				UINT16 y = (UINT16)cy << 3;
				if (Abs16((INT16)tx - (INT16)x) + Abs16((INT16)ty - (INT16)y) >= rt_fire_min &&
				    RT_LOSFast(x, y, rt_los_target, &los_ok)) { found = 1; break; }
			}
		} else if (cx >= gx0 && cx <= gx1 && cy >= gy0 && cy <= gy1) {
			found = 1;
			break;
		}
		if ((UINT8)(head - tail - 1) < 4) continue; /* ring nearly full (never in practice) */
		idx = rt_row[cy] + cx;
		/* up / down: bit1 = can step down from that cell */
		if (cy) {
			nidx = idx - rt_w;
			if (!rt_from[nidx] && (rt_walk[nidx] & 2)) {
				rt_from[nidx] = 1;
				rt_qx[tail] = cx; rt_qy[tail] = (UINT8)(cy - 1); tail++;
			}
		}
		if (rt_walk[idx] & 2) {
			nidx = idx + rt_w;
			if (!rt_from[nidx]) {
				rt_from[nidx] = 2;
				rt_qx[tail] = cx; rt_qy[tail] = (UINT8)(cy + 1); tail++;
			}
		}
		/* left / right: bit0 = can step right from that cell */
		if (cx) {
			nidx = idx - 1;
			if (!rt_from[nidx] && (rt_walk[nidx] & 1)) {
				rt_from[nidx] = 3;
				rt_qx[tail] = (UINT8)(cx - 1); rt_qy[tail] = cy; tail++;
			}
		}
		if (rt_walk[idx] & 1) {
			nidx = idx + 1;
			if (!rt_from[nidx]) {
				rt_from[nidx] = 4;
				rt_qx[tail] = (UINT8)(cx + 1); rt_qy[tail] = cy; tail++;
			}
		}
	}
	if (want_i == -3 && best_d != 0xFF) {
		found = 1;
		cx = bx;
		cy = by;
	}
	rt_len = 0;
	rt_pos = 0;
	if (door_on_route) *door_on_route = -1;
	if (route_cost) *route_cost = 0;
	if (!found) return 0;

	/* Walk back goal -> start: length, doors crossed, then fill waypoints. */
	{
		UINT8 fx = cx;
		UINT8 fy = cy;
		len = 0;
		while (rt_from[rt_row[cy] + cx] != 5) {
			UINT16 x = (UINT16)cx << 3;
			UINT16 y = (UINT16)cy << 3;
			for (i = 0; i != (door_on_route || route_cost ? rt_ndoors : 0); ++i) {
				INT16 dx = (INT16)rt_door_x[i] - (INT16)x;
				INT16 dy = (INT16)rt_door_y[i] - (INT16)y;
				if (dx < 13 && dx + (INT16)rt_door_w[i] > 3 && dy < 13 && dy + (INT16)rt_door_h[i] > 3) {
					if (door_on_route) *door_on_route = (INT8)i; /* last set = nearest to start */
					if (!(seen & (1 << i))) {
						seen |= (UINT8)(1 << i);
						if (route_cost) *route_cost = (UINT8)(*route_cost + rt_door_cost[i]);
					}
				}
			}
			d = (UINT8)(rt_from[rt_row[cy] + cx] - 1);
			cx = (UINT8)(cx - rt_dx[d]);
			cy = (UINT8)(cy - rt_dy[d]);
			len++;
		}
		/* waypoint 0 = start tile (re-align), then the route */
		cx = fx;
		cy = fy;
		{
			UINT8 total = (UINT8)(len + 1);
			UINT8 k = total;
			while (k) {
				k--;
				if (k < RT_PATH_MAX) {
					rt_px[k] = cx;
					rt_py[k] = cy;
				}
				if (k == 0) break;
				d = (UINT8)(rt_from[rt_row[cy] + cx] - 1);
				cx = (UINT8)(cx - rt_dx[d]);
				cy = (UINT8)(cy - rt_dy[d]);
			}
			rt_len = total < RT_PATH_MAX ? total : RT_PATH_MAX;
		}
	}
	return 1;
}

#endif /* level tests */
