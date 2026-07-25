#ifndef STATE_GAME_H
#define STATE_GAME_H

#include "Rooms.h"

extern UINT8 pending_room_transition;
extern UINT8 pending_electric_pickup;
void StartRoomEnemyWave(void);
void SyncGameHud(void);
UINT8 SafeTranslateSprite(Sprite* sprite, INT8 x, INT8 y);
UINT8 EnemyMoveWithWallAvoidance(Sprite* enemy, INT16 dx, INT16 dy);

#endif
