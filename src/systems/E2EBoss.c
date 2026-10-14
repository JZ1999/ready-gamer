#include "Banks/SetAutoBank.h"

#include "E2E.h"

#if E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR

#include "ZGBMain.h"
#include "Keys.h"
#include "SpriteManager.h"
#include "Scroll.h"
#include "BossRun.h"

/* Boss-run part of the e2e autopilot (split from E2E.c: ROM bank space).
 * Like the rest of the bot it only chooses buttons and reads game state. */

extern UINT8 boss_run_shoot_cooldown; /* BossRunPlayer.c (read only) */

/* Is the 16 px-tall player band starting at pixel row y blocked at tile col? */
static UINT8 E2E_BRBlocked(UINT16 col, INT16 y) {
	/* BossRunCheckCollision uses the full 16x16 box (no inset): rows
	 * y>>3 .. (y+15)>>3 (2 or 3 tiles). */
	if (y < 8 || y > (BOSSRUN_MAP_PIXELS_H - 24)) return 1;
	return BossRunTileBlocked(col, (UINT16)y >> 3) || BossRunTileBlocked(col, (UINT16)(y + 8) >> 3) ||
	       BossRunTileBlocked(col, (UINT16)(y + 15) >> 3);
}

/*
 * Boss run (auto-scroll, 1 HP): the camera pushes from the left and a wall
 * pillar pins (and hurts) a player who is behind it, so: thread the gap of
 * the next pillar ahead, sidestep the bullet fired along our row every
 * ~1.5 s, keep away from viruses (shoot the ones lined up ahead), and hold
 * the left-middle of the screen.
 */
static UINT8 E2E_BRFree(UINT16 x, INT16 y) {
	/* band free at our column and the next two (so a dodge doesn't walk into a pillar) */
	UINT16 c = (UINT16)(x >> 3);
	return !E2E_BRBlocked(c, y) && !E2E_BRBlocked(c + 1, y) && !E2E_BRBlocked(c + 2, y);
}

UINT8 E2E_BossRunBot(Sprite* p) BANKED {
	UINT8 i;
	Sprite* spr;
	UINT8 k = 0;
	INT16 py = (INT16)p->y;
	INT16 ty = py;
	UINT16 want_x = (UINT16)(scroll_x + 72); /* buffer from the camera edge (pillars crush) */
	UINT16 col0 = (UINT16)((p->x + 16) >> 3);
	UINT16 c;
	UINT16 gap_col = 0;
	UINT8 need_gap = 0;
	UINT8 urgent;
	INT16 vir_ty = 0x7FFF; /* row to line up with a virus ahead */
	/* 1) next pillar within 6 tiles that blocks our band: aim for its gap */
	for (c = col0; c != col0 + 6; ++c) {
		if (E2E_BRBlocked(c, py)) {
			INT16 best = 0x7FFF;
			INT16 y;
			for (y = 8; y <= (INT16)(BOSSRUN_MAP_PIXELS_H - 24); ++y) {
				INT16 dd = y > py ? y - py : py - y;
				if (!E2E_BRBlocked(c, y) && !E2E_BRBlocked(c + 1, y) && dd < best) { best = dd; ty = y; }
			}
			need_gap = 1;
			gap_col = c;
			break;
		}
	}
	/* camera close behind and a pillar ahead: the gap beats everything */
	urgent = need_gap && (INT16)(gap_col << 3) - (INT16)(p->x + 16) < 12 && (INT16)p->x - (INT16)scroll_x < 32;
	/* 2) bullets / viruses coming along our row: step out of the lane */
	SPRITEMANAGER_ITERATE(i, spr) {
		INT16 dy;
		INT16 dx;
		UINT8 threat = 0;
		if (spr->type != BossBullet && spr->type != BasicVirus) continue;
		dx = (INT16)spr->x - (INT16)p->x;
		dy = (INT16)spr->y - py;
		if (spr->type == BasicVirus) {
			/* lined up ahead: shoot (3 HP; it only creeps ~27 px per reload) */
			if (dx > 0 && dx < 88 && dy > -10 && dy < 14) {
				if (boss_run_shoot_cooldown == 0) return (UINT8)(J_RIGHT | J_B);
				/* reloading and it's close: get out of its row (it re-aligns
				 * slowly, 1 px per ~11 updates), never wait for contact */
				if (dx >= 40) continue;
				threat = 1;
			}
			/* ahead but off our row: line up with it (if that band is clear) */
			if (dx > 8 && dx < 120 && !urgent && E2E_BRFree(p->x, (INT16)spr->y - 2)) {
				vir_ty = (INT16)spr->y - 2;
				continue;
			}
			if (dx > -24 && dx < 24 && dy > -20 && dy < 24) threat = 1;
		} else if (dx > -(INT16)spr->coll_w - 2 && dx < 96 && dy >= -(INT16)spr->coll_h - 1 && dy <= (INT16)p->coll_h + 1) {
			threat = 1;
		}
		if (threat && !urgent) {
			/* Clear of the threat's rows: player box (16 px) entirely above
			 * or below its box (bullet 8 px, virus hitbox 8 px) + 1 px. */
			/* ZGB CheckCollision is inclusive (touching = hit): keep a full
			 * hitbox + 2 px away, using the sprites' real collision sizes. */
			INT16 above = (INT16)spr->y - (INT16)p->coll_h - 2;
			INT16 below = (INT16)spr->y + (INT16)spr->coll_h + 2;
			UINT8 a_ok = E2E_BRFree(p->x, above);
			UINT8 b_ok = E2E_BRFree(p->x, below);
			INT16 da = py > above ? py - above : above - py;
			INT16 db = py > below ? py - below : below - py;
			if (need_gap && ty != py) { /* prefer the gap's side */
				if (ty < py && a_ok) b_ok = 0;
				else if (ty > py && b_ok) a_ok = 0;
			}
			if (a_ok && (!b_ok || da <= db)) ty = above;
			else if (b_ok) ty = below;
			/* else: nowhere clear - hold (the gap logic keeps us moving) */
			need_gap = 0; /* dodge first, re-aim next frame */
			break;
		}
	}
	if (vir_ty != 0x7FFF && ty == py && !need_gap) ty = vir_ty;
	if (ty < py) k |= J_UP;
	else if (ty > py) k |= J_DOWN;
	if (need_gap) {
		/* lined up with the gap: go through; else only drift right if clear */
		if (!(k & (J_UP | J_DOWN)) || !E2E_BRBlocked((UINT16)((p->x + 17) >> 3), py)) k |= J_RIGHT;
	} else if (p->x + 4 < want_x) {
		if (!E2E_BRBlocked((UINT16)((p->x + 17) >> 3), py)) k |= J_RIGHT;
	} else if (p->x > want_x + 8) {
		k |= J_LEFT;
	}
	return k;
}

#endif
