#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "DoorCollision.h"
#include "StateGame.h"

// True if box overlaps a closed door's tile footprint (every 8x8 map tile its
// drawn pixels touch). Doors sit in their wall column (DOOR_ON_TILE_X), so the
// footprint is exactly as thick as the wall. It also covers the full wall gap
// around short doors (rooms 0-2: 24px gap, 16px door).
static UINT8 BoxOverlapsClosedDoorTiles(INT16 x, INT16 y, UINT8 w, UINT8 h) {
    UINT8 i;
    Sprite* spr;
    INT16 left, top, right, bottom;
    SPRITEMANAGER_ITERATE(i, spr) {
        // Opened doors are removed from the sprite list, but check the state
        // anyway so a door is only solid while closed.
        if (IsDoorType(spr->type) && spr->custom_data[CD_DOOR_STATE] == 0) {
            // Snap the drawn rect outwards to the 8px tile grid.
            left   = ((INT16)spr->x + DOOR_HITBOX_X) & ~7;
            top    = ((INT16)spr->y + DOOR_HITBOX_Y) & ~7;
            right  = (((INT16)spr->x + DOOR_HITBOX_X + DOOR_HITBOX_W + 7) & ~7);
            bottom = (((INT16)spr->y + DOOR_HITBOX_Y + DOOR_HITBOX_H + 7) & ~7);
            if (x < right && left < x + w && y < bottom && top < y + h) return 1;
        }
    }
    return 0;
}

UINT8 SpriteBlockedByClosedDoor(Sprite* sprite, INT8 dx, INT8 dy) BANKED {
    // Same inset box CheckEdgeMapCollision (ZGBMain.c) uses against walls, so
    // a closed door blocks exactly like the wall tiles it plugs: sprites slide
    // along the wall past the door instead of snagging on a protruding edge.
    INT16 x = (INT16)sprite->x + WALL_COLLISION_MARGIN;
    INT16 y = (INT16)sprite->y + WALL_COLLISION_MARGIN;
    UINT8 w = sprite->coll_w - (WALL_COLLISION_MARGIN * 2);
    UINT8 h = sprite->coll_h - (WALL_COLLISION_MARGIN * 2);

    // Already inside a door (no spawn point overlaps one, so this should not
    // happen): let it walk out instead of freezing in place forever.
    if (BoxOverlapsClosedDoorTiles(x, y, w, h)) {
        return 0;
    }

    return BoxOverlapsClosedDoorTiles(x + dx, y + dy, w, h);
}
