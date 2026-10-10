#ifndef STATE_GAME_H
#define STATE_GAME_H

#include "Rooms.h"

extern UINT8 pending_room_transition;
extern UINT8 pending_electric_pickup;
void StartRoomEnemyWave(void);
void SyncGameHud(void);
// Room wall checks shrink the sprite coll box by this many px per side
// (corner forgiveness, see ZGBMain.c CheckEdgeMapCollision).
#define WALL_COLLISION_MARGIN 3
UINT8 SafeTranslateSprite(Sprite* sprite, INT8 x, INT8 y);
UINT8 EnemyTranslateSprite(Sprite* enemy, INT8 dx, INT8 dy);
UINT8 EnemyMoveWithWallAvoidance(Sprite* enemy, INT16 dx, INT16 dy);

#endif
