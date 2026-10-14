#ifndef E2EPLAN_H
#define E2EPLAN_H

/* Whole-map route planner for the level e2e tests (E2EPlan.c, own ROM bank).
 * Internal to the autopilot: only E2E.c includes this. */

#include <gbdk/platform.h>
#include "SpriteManager.h"

#define RT_PATH_MAX 96
#define RT_MAX_DOORS 4

#define RT_MAXW 50
#define RT_MAXH 18
extern UINT8 rt_walk[RT_MAXW * RT_MAXH]; /* bit0: can step right, bit1: can step down (player box) */
extern UINT16 rt_row[RT_MAXH];
extern UINT8 rt_w;
extern UINT8 rt_h;
extern UINT8 rt_px[RT_PATH_MAX];
extern UINT8 rt_py[RT_PATH_MAX];
extern UINT8 rt_len;
extern UINT8 rt_pos;
extern UINT8 rt_room;
extern UINT16 rt_tx;
extern UINT16 rt_ty;
extern UINT16 rt_age;
extern UINT8 rt_ndoors;
extern UINT16 rt_door_x[RT_MAX_DOORS];
extern UINT16 rt_door_y[RT_MAX_DOORS];
extern UINT8 rt_plan_cd;
extern UINT8 rt_throttled;
extern Sprite* rt_los_target;
extern UINT8 rt_fire_min;

void RT_SnapshotDoors(void) BANKED;
Sprite* RT_DoorAt(UINT16 x, UINT16 y) BANKED;
UINT8 RT_DoorTile(UINT8 tx, UINT8 ty) BANKED;
void RT_BuildWalk(Sprite* p) BANKED;
UINT8 RT_LOSFast(UINT16 x, UINT16 y, Sprite* e, UINT8* ok) BANKED;
UINT8 RT_Plan(Sprite* p, UINT16 tx, UINT16 ty, UINT8 r, INT8 want_i, UINT8 passable,
              INT8* door_on_route, UINT8* route_cost) BANKED;

#endif
