#ifndef ROOMS_H
#define ROOMS_H

#include <gbdk/platform.h>

typedef struct RoomDef RoomDef;

// Max slots in the rooms[] table (can be larger than room_count)
#define MAX_ROOMS 5

extern UINT8 current_room;
extern const UINT8 room_count;

const RoomDef* GetCurrentRoom(void);
UINT8 GetCurrentRoomSpawnPointCount(void);
void GetRandomSpawnPosition(UINT8* x, UINT8* y);
void EnsureRoomSpawnPoints(void);

void GetRandomSpawnPositionFromTable(UINT8* x, UINT8* y);
void EnsureRoomSpawnPointsFromTable(void);

void InitRoomGraphics(UINT8 room_index);
void SpawnRoomEntities(UINT8 room_index);
void LoadRoom(UINT8 room_index);
void LoadNextRoom(void);

void InitRoomScrollFromTable(UINT8 room_index);
void LoadRoomFromTable(UINT8 room_index);
void FinalizeRoomScrollFromTable(UINT8 room_index);
void SpawnRoomFromTable(UINT8 room_index);
void CenterScrollOnPlayerFromTable(void);
UINT8 GetRoomTileFromTable(UINT16 x, UINT16 y);
void EnsureRoomScrollMap(void);

#endif
