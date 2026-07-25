#include "Banks/SetAutoBank.h"

#include <rand.h>

#include "Rooms.h"
#include "ZGBMain.h"
#include "Scroll.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "MapInfo.h"
#include "BankManager.h"
#include <string.h>

extern UINT8 last_tile_loaded;
extern UINT8 last_bg_pal_loaded;

IMPORT_MAP(map);
IMPORT_MAP(map2);
IMPORT_MAP(map3);
IMPORT_MAP(map4);
IMPORT_MAP(map5);

#define ARRAY_LEN(A) (sizeof(A) / sizeof((A)[0]))

typedef struct {
    UINT8 bank;
    struct MapInfo* map;
} RoomMap;

typedef struct {
    UINT16 x;
    UINT16 y;
    UINT8 cost;
} DoorPlacement;

typedef struct {
    UINT16 x;
    UINT16 y;
} SpawnPointPlacement;

typedef struct {
    UINT16 x;
    UINT16 y;
} PortalPlacement;

typedef struct {
    UINT16 x;
    UINT16 y;
} PickupPlacement;

struct RoomDef {
    RoomMap scroll;
    UINT16 player_x;
    UINT16 player_y;
    const DoorPlacement* doors;
    UINT8 door_count;
    const SpawnPointPlacement* spawn_points;
    UINT8 spawn_point_count;
    const PortalPlacement* portals;
    UINT8 portal_count;
    const PickupPlacement* electricity_pickups;
    UINT8 electricity_pickup_count;
    const PickupPlacement* coin_pickups;
    UINT8 coin_pickup_count;
};

#define ROOM_MAP(MAP) { BANK(MAP), &MAP }

static const DoorPlacement room0_doors[] = {
    { 115, 52, 1 },
};

static const SpawnPointPlacement room0_spawns[] = {
    { 20, 20 },
    { 100, 80 },
};

static const PortalPlacement room0_portals[] = {
    { 135, 52 },
};

static const DoorPlacement room1_doors[] = {
    { 115, 52, 1 },
    { 115, 100, 1 },
};

static const SpawnPointPlacement room1_spawns[] = {
    { 20, 25 },
    { 100, 80 },
    { 50, 90 },
};

static const PortalPlacement room1_portals[] = {
    { 135, 52 },
};

static const PickupPlacement room1_electricity[] = {
    { 132, 96 },
};

static const DoorPlacement room2_doors[] = {
    { 115, 52, 1 },
};

static const SpawnPointPlacement room2_spawns[] = {
    { 20, 20 },
    { 100, 80 },
    { 50, 90 },
};

static const PortalPlacement room2_portals[] = {
    { 135, 52 },
};

static const PickupPlacement room2_coins[] = {
    { 72, 100 },
};

static const DoorPlacement room3_doors[] = {
    { 232, 24, 1 },
};

static const SpawnPointPlacement room3_spawns[] = {
    { 40, 30 },
    { 80, 88 },
    { 200, 112 },
    { 48, 120 },
};

static const PortalPlacement room3_portals[] = {
    { 280, 24 },
};

static const DoorPlacement room4_doors[] = {
    { 264, 24, 1 },
};

static const SpawnPointPlacement room4_spawns[] = {
    { 48, 120 },
    { 160, 40 },
    { 240, 112 },
    { 80, 64 },
};

static const PickupPlacement room4_coins[] = {
    { 144, 64 },
};

/*
 * To add a room:
 *   1. Create res/mapN.gbm
 *   2. IMPORT_MAP(mapN) above
 *   3. Add room data arrays
 *   4. Append a RoomDef below
 *   5. Increment room_count / MAX_ROOMS
 */
static const RoomDef rooms[MAX_ROOMS] = {
    {
        ROOM_MAP(map),
        90, 50,
        room0_doors, ARRAY_LEN(room0_doors),
        room0_spawns, ARRAY_LEN(room0_spawns),
        room0_portals, ARRAY_LEN(room0_portals),
        NULL, 0,
        NULL, 0,
    },
    {
        ROOM_MAP(map2),
        90, 50,
        room1_doors, ARRAY_LEN(room1_doors),
        room1_spawns, ARRAY_LEN(room1_spawns),
        room1_portals, ARRAY_LEN(room1_portals),
        room1_electricity, ARRAY_LEN(room1_electricity),
        NULL, 0,
    },
    {
        ROOM_MAP(map3),
        90, 50,
        room2_doors, ARRAY_LEN(room2_doors),
        room2_spawns, ARRAY_LEN(room2_spawns),
        room2_portals, ARRAY_LEN(room2_portals),
        NULL, 0,
        room2_coins, ARRAY_LEN(room2_coins),
    },
    {
        ROOM_MAP(map4),
        48, 40,
        room3_doors, ARRAY_LEN(room3_doors),
        room3_spawns, ARRAY_LEN(room3_spawns),
        room3_portals, ARRAY_LEN(room3_portals),
        NULL, 0,
        NULL, 0,
    },
    {
        ROOM_MAP(map5),
        48, 120,
        room4_doors, ARRAY_LEN(room4_doors),
        room4_spawns, ARRAY_LEN(room4_spawns),
        NULL, 0,
        NULL, 0,
        room4_coins, ARRAY_LEN(room4_coins),
    },
};

UINT8 current_room = 0;
const UINT8 room_count = 5;

static const RoomDef* GetRoomDef(UINT8 room_index) {
    if (room_index >= room_count) {
        return NULL;
    }
    return &rooms[room_index];
}

const RoomDef* GetCurrentRoom(void) {
    const RoomDef* room = GetRoomDef(current_room);
    if (room == NULL) {
        return &rooms[0];
    }
    return room;
}

UINT8 GetCurrentRoomSpawnPointCount(void) {
    return GetCurrentRoom()->spawn_point_count;
}

static void FinalizeRoomScroll(const RoomDef* room) {
    if (room == NULL || scroll_target == NULL) {
        return;
    }

    FinalizeRoomScrollFromTable(current_room);
}

static void InitRoomScroll(UINT8 room_index) {
    InitRoomScrollFromTable(room_index);
}

static void SpawnDoor(const DoorPlacement* door) {
    Sprite* sprite = SpriteManagerAdd(Door, door->x, door->y);
    if (sprite) {
        sprite->custom_data[CD_DOOR_STATE] = 0;
        sprite->custom_data[CD_DOOR_COST] = door->cost;
    }
}

static void SpawnDoors(const RoomDef* room) {
    UINT8 i;

    for (i = 0; i != room->door_count; ++i) {
        SpawnDoor(&room->doors[i]);
    }
}

static void SpawnSpawnPoints(const RoomDef* room) {
    UINT8 i;

    for (i = 0; i != room->spawn_point_count; ++i) {
        SpriteManagerAdd(SpawnPoint, room->spawn_points[i].x, room->spawn_points[i].y);
    }
}

static void SpawnPortals(const RoomDef* room) {
    UINT8 i;

    for (i = 0; i != room->portal_count; ++i) {
        SpriteManagerAdd(NextLevelPortal, room->portals[i].x, room->portals[i].y);
    }
}

static void SpawnElectricityPickups(const RoomDef* room) {
    UINT8 i;

    for (i = 0; i != room->electricity_pickup_count; ++i) {
        SpriteManagerAdd(ElectricityPickup, room->electricity_pickups[i].x, room->electricity_pickups[i].y);
    }
}

static void SpawnCoinPickups(const RoomDef* room) {
    UINT8 i;

    for (i = 0; i != room->coin_pickup_count; ++i) {
        SpriteManagerAdd(CoinsPickup, room->coin_pickups[i].x, room->coin_pickups[i].y);
    }
}

static void SetupRoomEntities(const RoomDef* room) {
    SpawnDoors(room);
    SpawnSpawnPoints(room);
    SpawnPortals(room);
    SpawnElectricityPickups(room);
    SpawnCoinPickups(room);
}

void GetRandomSpawnPosition(UINT8* x, UINT8* y) {
    const RoomDef* room = GetCurrentRoom();
    const SpawnPointPlacement* spawn;
    UINT8 index;

    if (room->spawn_point_count == 0) {
        *x = 0;
        *y = 0;
        return;
    }

    index = rand() % room->spawn_point_count;
    spawn = &room->spawn_points[index];
    *x = (UINT8)spawn->x;
    *y = (UINT8)spawn->y;
}

void EnsureRoomSpawnPoints(void) {
    const RoomDef* room = GetCurrentRoom();
    UINT8 i;
    UINT8 spr_idx;
    Sprite* spr;
    UINT8 spawn_point_count = 0;

    SPRITEMANAGER_ITERATE(spr_idx, spr) {
        if (spr->type == SpawnPoint) {
            spawn_point_count++;
        }
    }

    if (spawn_point_count >= room->spawn_point_count) {
        return;
    }

    for (i = spawn_point_count; i != room->spawn_point_count; ++i) {
        SpriteManagerAdd(SpawnPoint, room->spawn_points[i].x, room->spawn_points[i].y);
    }
}

void InitRoomGraphics(UINT8 room_index) {
    const RoomDef* room = GetRoomDef(room_index);

    if (room == NULL) {
        return;
    }

    current_room = room_index;
    scroll_target = NULL;

    last_tile_loaded = 0;
    last_bg_pal_loaded = 0;
    scroll_offset_x = 0;
    scroll_offset_y = 0;

    InitRoomScroll(room_index);
}

void SpawnRoomEntities(UINT8 room_index) {
    const RoomDef* room = GetRoomDef(room_index);

    if (room == NULL) {
        return;
    }

    current_room = room_index;
    SpriteManagerReset();
    scroll_target = SpriteManagerAdd(SpritePlayer, room->player_x, room->player_y);
    SetupRoomEntities(room);
    FinalizeRoomScroll(room);
}

void LoadRoom(UINT8 room_index) {
    InitRoomGraphics(room_index);
    SpawnRoomEntities(room_index);
}

void LoadNextRoom(void) {
    if ((UINT8)(current_room + 1) >= room_count) {
        return;
    }

    LoadRoom(current_room + 1);
}
