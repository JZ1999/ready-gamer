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
    /* Indices into this room's spawn_points[] that this door unlocks when
     * opened. NULL/0 = door doesn't gate spawns at all (existing behavior,
     * every spawn point stays usable regardless of door state). */
    const UINT8* unlock_spawn_indices;
    UINT8 unlock_spawn_count;
} DoorPlacement;

typedef struct {
    UINT16 x;
    UINT16 y;
    /* 1 = this spawn starts locked (excluded from GetRandomSpawnPosition)
     * until a door that lists it in its unlock_spawn_indices opens. */
    UINT8 initially_locked;
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
    { 115, 52, 10, NULL, 0 },
};

static const SpawnPointPlacement room0_spawns[] = {
    { 20, 20, 0 },
    { 100, 80, 0 },
};

static const PortalPlacement room0_portals[] = {
    { 135, 52 },
};

static const DoorPlacement room1_doors[] = {
    { 115, 52, 10, NULL, 0 },
    { 115, 100, 10, NULL, 0 },
};

static const SpawnPointPlacement room1_spawns[] = {
    { 20, 25, 0 },
    { 100, 80, 0 },
    { 50, 90, 0 },
};

static const PortalPlacement room1_portals[] = {
    { 135, 52 },
};

static const PickupPlacement room1_electricity[] = {
    { 132, 96 },
};

static const DoorPlacement room2_doors[] = {
    { 115, 52, 10, NULL, 0 },
};

static const SpawnPointPlacement room2_spawns[] = {
    { 20, 20, 0 },
    { 100, 80, 0 },
    { 50, 90, 0 },
};

static const PortalPlacement room2_portals[] = {
    { 135, 52 },
};

static const PickupPlacement room2_coins[] = {
    { 72, 100 },
};

static const DoorPlacement room3_doors[] = {
    { 232, 24, 10, NULL, 0 },
};

static const SpawnPointPlacement room3_spawns[] = {
    { 40, 30, 0 },
    { 80, 88, 0 },
    { 200, 112, 0 },
    { 48, 120, 0 },
};

static const PortalPlacement room3_portals[] = {
    { 280, 24 },
};

/*
 * map5 (formerly the room6 draft) layout, from GBMB tile coordinates
 * (col,row) * 8 = pixels. Explicit progression per the user (overrides the
 * earlier reachability-based guess — see SESSION_NOTES.md history):
 *   - Only S1/S2/S3 start unlocked. Everything else starts locked.
 *   - D3 (bottom-left, right next to player start) is the "first door":
 *     opening it locks S1/S2/S3 back up and unlocks S5/S7/S8.
 *   - D1 (top-left) is the "second door": opening it unlocks S4/S6/S9
 *     (locking everything else, including S5/S7/S8).
 *   - D2 (top-right) doesn't gate any spawn — it gates the path to the
 *     portal instead; the portal needs all 3 doors open to be reachable.
 */
static const UINT8 room4_door_d3_unlocks[] = { 4, 6, 7 }; /* S5, S7, S8 */
static const UINT8 room4_door_d1_unlocks[] = { 3, 5, 8 }; /* S4, S6, S9 */

static const DoorPlacement room4_doors[] = {
    { 26 * 8, 1 * 8, 10, room4_door_d1_unlocks, ARRAY_LEN(room4_door_d1_unlocks) }, /* D1 — second */
    { 38 * 8, 1 * 8, 10, NULL, 0 }, /* D2 — doesn't gate spawns */
    { 12 * 8, 15 * 8, 10, room4_door_d3_unlocks, ARRAY_LEN(room4_door_d3_unlocks) }, /* D3 — first */
};

static const SpawnPointPlacement room4_spawns[] = {
    { 6 * 8, 2 * 8, 0 },   /* S1 — unlocked at start, locked once D3 opens */
    { 9 * 8, 2 * 8, 0 },   /* S2 — unlocked at start, locked once D3 opens */
    { 3 * 8, 2 * 8, 0 },   /* S3 — unlocked at start, locked once D3 opens */
    { 35 * 8, 6 * 8, 1 },  /* S4 — locked until D1 opens */
    { 23 * 8, 6 * 8, 1 },  /* S5 — locked until D3 opens */
    { 28 * 8, 11 * 8, 1 }, /* S6 — locked until D1 opens */
    { 23 * 8, 11 * 8, 1 }, /* S7 — locked until D3 opens */
    { 23 * 8, 15 * 8, 1 }, /* S8 — locked until D3 opens */
    { 35 * 8, 15 * 8, 1 }, /* S9 — locked until D1 opens */
};

/* Exit portal — only reachable once all 3 doors are open. */
static const PortalPlacement room4_portals[] = {
    { 46 * 8, 15 * 8 },
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
        3 * 8, 15 * 8,
        room4_doors, ARRAY_LEN(room4_doors),
        room4_spawns, ARRAY_LEN(room4_spawns),
        room4_portals, ARRAY_LEN(room4_portals),
        NULL, 0,
        NULL, 0,
    },
};

UINT8 current_room = 0;
const UINT8 room_count = 5;

/* Runtime lock state for the current room's spawn points, indexed the same
 * as that room's spawn_points[] table. Reset on every room load. map5's 9
 * spawn points are the largest in use today; 10 leaves a little headroom. */
#define MAX_ROOM_SPAWN_POINTS 10
static UINT8 spawn_locked[MAX_ROOM_SPAWN_POINTS];

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

static UINT8 IsSpawnIndexLocked(UINT8 index) {
    if (index >= MAX_ROOM_SPAWN_POINTS) {
        return 0; /* out of tracked range — treat as unlocked rather than unusable */
    }
    return spawn_locked[index];
}

void GetRandomSpawnPosition(UINT16* x, UINT16* y) {
    const RoomDef* room = GetCurrentRoom();
    const SpawnPointPlacement* spawn;
    UINT8 unlocked_indices[MAX_ROOM_SPAWN_POINTS];
    UINT8 unlocked_count = 0;
    UINT8 i;
    UINT8 index;

    if (room->spawn_point_count == 0) {
        *x = 0;
        *y = 0;
        return;
    }

    for (i = 0; i != room->spawn_point_count && i != MAX_ROOM_SPAWN_POINTS; ++i) {
        if (!IsSpawnIndexLocked(i)) {
            unlocked_indices[unlocked_count] = i;
            unlocked_count++;
        }
    }

    if (unlocked_count == 0) {
        /* Safety net: a misconfigured door unlock list could lock every
         * spawn point in the room. Fall back to "all usable" instead of
         * permanently soft-locking the wave. */
        index = rand() % room->spawn_point_count;
    } else {
        index = unlocked_indices[rand() % unlocked_count];
    }

    spawn = &room->spawn_points[index];
    *x = spawn->x;
    *y = spawn->y;
}

static void ResetRoomSpawnLocks(const RoomDef* room) {
    UINT8 i;
    UINT8 count = room->spawn_point_count;

    if (count > MAX_ROOM_SPAWN_POINTS) {
        count = MAX_ROOM_SPAWN_POINTS;
    }

    for (i = 0; i != count; ++i) {
        spawn_locked[i] = room->spawn_points[i].initially_locked;
    }
    for (; i != MAX_ROOM_SPAWN_POINTS; ++i) {
        spawn_locked[i] = 0;
    }
}

/**
 * Called when a door finishes opening. Looks up which DoorPlacement matches
 * the door's world position; if that door defines an unlock list, spawn
 * points in the list become unlocked and every other spawn point in the
 * room is locked. Doors with an empty/NULL list (the default) don't touch
 * lock state at all, so rooms that don't use this feature are unaffected.
 */
void ApplyDoorSpawnUnlocks(UINT16 door_x, UINT16 door_y) {
    const RoomDef* room = GetCurrentRoom();
    const DoorPlacement* door = NULL;
    UINT8 i, j;
    UINT8 unlocked;

    for (i = 0; i != room->door_count; ++i) {
        if (room->doors[i].x == door_x && room->doors[i].y == door_y) {
            door = &room->doors[i];
            break;
        }
    }

    if (door == NULL || door->unlock_spawn_count == 0) {
        return;
    }

    for (i = 0; i != room->spawn_point_count && i != MAX_ROOM_SPAWN_POINTS; ++i) {
        unlocked = 0;
        for (j = 0; j != door->unlock_spawn_count; ++j) {
            if (door->unlock_spawn_indices[j] == i) {
                unlocked = 1;
                break;
            }
        }
        spawn_locked[i] = unlocked ? 0 : 1;
    }
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
    ResetRoomSpawnLocks(room);
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
