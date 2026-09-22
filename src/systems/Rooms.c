#include "Banks/SetAutoBank.h"

#include <rand.h>

#include "Rooms.h"
#include "ZGBMain.h"
#include "Scroll.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "SpriteBudget.h"
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

/*
 * map4 rework v4: user-drawn in GBMB by hand (screenshot with color-coded
 * markup: green=player start, red=spawns, purple=portal, black=door). Door
 * tile coordinates (col,row) read directly off GBMB's status bar, then
 * shifted 1 tile left/up per the user's request. player/spawn/portal tiles
 * read from the screenshot against a render of the saved map4.gbm tile data
 * (only need to land on a floor tile). Doors: P1=(3,10), P2=(8,4),
 * P3=(32,7).
 *
 * Spawn-lock scheme (inferred from the drawing + a BFS reachability check
 * against the real tile data, same technique as map5):
 *   - Only S5 and S7 start unlocked — the two spawns already reachable from
 *     the player start with all 3 doors closed (everything else is either
 *     physically gated by a door, or deliberately held back).
 *   - P1 (south, right by the S1/S6/S8/bottom-row area) is the only door
 *     that BFS shows actually gates anything physically: opening it reveals
 *     S1/S6/S8, so it's grouped with the rest of that lower region
 *     (S9/S10/S11 too) as one wave.
 *   - P3 (top-right, sitting right next to the S2-S5 cluster) unlocks the
 *     rest of that cluster (S2/S3/S4) — S5 already active, the other 3 join
 *     once it's open. Note: S2/S3/S4 sit at (38,5)/(36,5)/(34,5), BELOW the
 *     near-solid row-3 wall band — BFS shows they actually need BOTH P1 AND
 *     P2 open (not just P3) to be physically reachable. That's fine, not a
 *     softlock: the portal already needs all 3 doors open, so a player who
 *     opens P3 first just kills that trio a bit later, once P1/P2 are open
 *     too — never permanently unreachable.
 *   - P2 doesn't gate any spawn on its own (BFS: opening it alone reveals
 *     nothing new) — same as map5's D2, it only clears part of the path
 *     (here, jointly with P1, toward S2/S3/S4 and the portal).
 *   - Portal needs all 3 doors open (BFS-confirmed) — same "all doors"
 *     convention as map5.
 * Doors use "replace" semantics (ApplyDoorSpawnUnlocks, see room4 below) so
 * opening P1 after P3 (or vice versa) re-locks the other's wave — same
 * accepted behavior as map5's D1/D3.
 *
 * Sprite-cutting fix + "move every spawn 1 right" (later pass): the bottom
 * cluster (S7-S11) and the portal were all sitting on the exact same row
 * (15), plus 3 always-alive door sprites and the portal itself never get
 * removed off-screen (lim_x/lim_y=255) — with several enemies alive at
 * once on that same row, the GB's 10-sprites-per-scanline hardware limit
 * got exceeded and sprites visually "cut" whenever the player's own sprite
 * shared that row too. Fixed by spreading the bottom cluster across rows
 * 14/15/16 instead of all on 15, so no more than 2-3 sprites ever compete
 * for the same scanline band.
 *
 * Simplified by the user (2026-09-05): since the number of unlocked spawns
 * doesn't change spawn *rate* (SpawnEnemies() spawns one enemy per timer
 * tick regardless of how many spawn points are unlocked — see
 * SESSION_NOTES.md), redundant spawns in each door's group were removed —
 * one per door is enough:
 *   - P1 still unlocks S1(4,4)/S6(12,10)/S8(4,15) (kept as-is, all 3 already
 *     needed for the room's original spread).
 *   - P2 unlocks only S4(33,4) now — S2/S3 removed entirely (were the same
 *     group, same door, redundant).
 *   - P3 unlocks only S9(7,14) now — S10/S11 removed entirely (were causing
 *     the scanline sprite-cutting on that row when clustered).
 * S5/S7 remain the only spawns unlocked at room load. Portal moved next to
 * S9 at (9,15). Portal still needs all 3 doors open.
 */
static const UINT8 room3_door_p1_unlocks[] = { 0, 3, 5 }; /* S1,S6,S8 */
static const UINT8 room3_door_p2_unlocks[] = { 1 };       /* S4 */
static const UINT8 room3_door_p3_unlocks[] = { 6 };       /* S9 */

static const DoorPlacement room3_doors[] = {
    { 3 * 8, 10 * 8, 10, room3_door_p1_unlocks, ARRAY_LEN(room3_door_p1_unlocks) }, /* P1 (moved 1 tile right: tile x 2 -> 3) */
    { 8 * 8, 4 * 8, 10, room3_door_p2_unlocks, ARRAY_LEN(room3_door_p2_unlocks) },  /* P2 */
    { 32 * 8, 7 * 8, 10, room3_door_p3_unlocks, ARRAY_LEN(room3_door_p3_unlocks) }, /* P3 */
};

static const SpawnPointPlacement room3_spawns[] = {
    { 4 * 8, 4 * 8, 1 },   /* S1 — locked until P1 opens */
    { 33 * 8, 4 * 8, 1 },  /* S4 — locked until P2 opens */
    { 37 * 8, 1 * 8, 0 },  /* S5 — the corner one, unlocked at start */
    { 12 * 8, 10 * 8, 1 }, /* S6 — locked until P1 opens */
    { 1 * 8, 15 * 8, 0 },  /* S7 — unlocked at start */
    { 4 * 8, 15 * 8, 1 },  /* S8 — locked until P1 opens */
    { 7 * 8, 15 * 8, 1 },  /* S9 — locked until P3 opens */
};

static const PortalPlacement room3_portals[] = {
    { 9 * 8, 15 * 8 }, /* moved next to S9 per user request */
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
        1 * 8, 1 * 8,
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
 * spawn points are the largest in use today (map4 was trimmed to 7); 12
 * leaves headroom for future rooms. */
#define MAX_ROOM_SPAWN_POINTS 12
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

/**
 * Makes the visible spawn-marker sprites match spawn_locked[]: one marker per
 * UNLOCKED spawn point, none for locked ones. Markers are purely visual
 * (enemy positions come from the room table), so hiding locked ones only
 * frees sprite-pool slots (see SpriteBudget.h) without touching gameplay.
 *
 * Idempotent: safe to call any time (room load, door open, wave clear). If an
 * add was refused because the pool was full, the next call retries it.
 */
static void SyncSpawnMarkers(const RoomDef* room) {
    UINT8 i;
    UINT8 spr_idx;
    Sprite* spr;
    UINT8 count = room->spawn_point_count;
    UINT8 has_marker[MAX_ROOM_SPAWN_POINTS];

    if (count > MAX_ROOM_SPAWN_POINTS) {
        count = MAX_ROOM_SPAWN_POINTS;
    }
    memset(has_marker, 0, sizeof(has_marker));

    /* Pass 1: match live markers to table entries by position. Locked ones
     * are marked for removal (deferred, safe mid-iteration). Never add here:
     * adding while iterating would shift the sprite vector under us. */
    SPRITEMANAGER_ITERATE(spr_idx, spr) {
        if (spr->type != SpawnPoint || spr->marked_for_removal) {
            continue;
        }
        for (i = 0; i != count; ++i) {
            if (spr->x == room->spawn_points[i].x && spr->y == room->spawn_points[i].y) {
                if (spawn_locked[i]) {
                    SpriteManagerRemove(spr_idx);
                } else {
                    has_marker[i] = 1;
                }
                break;
            }
        }
    }

    /* Pass 2: add the missing markers for unlocked spawn points. */
    for (i = 0; i != count; ++i) {
        if (!spawn_locked[i] && !has_marker[i]) {
            SafeSpriteAddLow(SpawnPoint, room->spawn_points[i].x, room->spawn_points[i].y);
        }
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
    /* Portal spawned before spawn points: GB hardware drops sprites past the
       10-per-scanline cap by OAM order (lowest index wins), and OAM order
       here follows add order. Exit portal must never lose that race to a
       spawn marker sharing its row. */
    SpawnDoors(room);
    SpawnPortals(room);
    SyncSpawnMarkers(room);
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

    /* Lock state changed: hide markers of newly locked spawns, show the
     * newly unlocked ones. */
    SyncSpawnMarkers(room);
}

void EnsureRoomSpawnPoints(void) {
    SyncSpawnMarkers(GetCurrentRoom());
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
