#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteData.h"
#include "Hurtbox.h"
#include "Rooms.h"

// Drawn-body rect of an enemy, relative to its sprite x/y.
typedef struct {
    UINT8 x, y, w, h;
} BodyBox;

// Per-type body from SpriteData.h. Unknown types fall back to the full
// 16x16 cell so a new enemy still hurts (never silently harmless).
static void GetEnemyBody(UINT8 type, BodyBox* box) {
    if (type == BasicVirus || type == TankVirus) {
        box->x = BASIC_BODY_X;  box->y = BASIC_BODY_Y;  box->w = BASIC_BODY_W;  box->h = BASIC_BODY_H;
    } else if (type == SpeedVirus) {
        box->x = SPEED_BODY_X;  box->y = SPEED_BODY_Y;  box->w = SPEED_BODY_W;  box->h = SPEED_BODY_H;
    } else if (type == ChargeVirus) {
        box->x = CHARGE_BODY_X; box->y = CHARGE_BODY_Y; box->w = CHARGE_BODY_W; box->h = CHARGE_BODY_H;
    } else {
        box->x = BOMBER_BODY_X; box->y = BOMBER_BODY_Y; box->w = BOMBER_BODY_W; box->h = BOMBER_BODY_H;
    }
}

// Same value as TILE_FULL_BRICK in ZGBMain.c (GetRoomTileFromTable returns
// normalized ids). Only full bricks block: partial bricks are half-height
// ledges the bodies can legitimately overlap across.
#define HURTBOX_WALL_TILE 1
// Sample spacing along the line; < 8 so a 1-tile wall can't be skipped.
#define WALL_SAMPLE_STEP 4

// 1 if a full-brick tile lies on the straight line between the two wall-box
// centers. Why: enemy wall boxes are 2x2 px but bodies are 16 px wide, so an
// enemy pressed against a 1-tile wall pokes its drawn body through it and
// would hurt a player standing on the other side (reported with Tank).
// Wall-box centers are never inside a full brick, so a clear line = same side.
static UINT8 WallBetween(Sprite* a, Sprite* b) {
    INT16 ax = (INT16)a->x + (a->coll_w >> 1);
    INT16 ay = (INT16)a->y + (a->coll_h >> 1);
    INT16 dx = ((INT16)b->x + (b->coll_w >> 1)) - ax;
    INT16 dy = ((INT16)b->y + (b->coll_h >> 1)) - ay;
    INT16 adx = dx < 0 ? -dx : dx;
    INT16 ady = dy < 0 ? -dy : dy;
    INT16 dist = adx > ady ? adx : ady;
    INT16 steps = dist / WALL_SAMPLE_STEP + 1;
    INT16 i;

    for (i = 1; i <= steps; i++) {
        INT16 sx = ax + (INT16)((dx * i) / steps);
        INT16 sy = ay + (INT16)((dy * i) / steps);
        if (sx < 0 || sy < 0) continue;  // off-map: GetRoomTileFromTable says 0 anyway
        if (GetRoomTileFromTable((UINT16)sx >> 3, (UINT16)sy >> 3) == HURTBOX_WALL_TILE) {
            return 1;
        }
    }
    return 0;
}

UINT8 PlayerTouchesEnemy(Sprite* player, Sprite* enemy) BANKED {
    BodyBox e;
    // INT16: sprite x/y are UINT16 map coords; signed math avoids wrap.
    INT16 px = (INT16)player->x + PLAYER_BODY_X;
    INT16 py = (INT16)player->y + PLAYER_BODY_Y;
    INT16 ex, ey;

    GetEnemyBody(enemy->type, &e);
    ex = (INT16)enemy->x + e.x;
    ey = (INT16)enemy->y + e.y;

    // Strict overlap: rects that only share an edge do not count, so a hit
    // needs at least one drawn pixel of each sprite on top of the other.
    if (!(px < ex + e.w && ex < px + PLAYER_BODY_W &&
          py < ey + e.h && ey < py + PLAYER_BODY_H)) {
        return 0;
    }
    // Overlap test first: the wall line check reads ROM tiles, only pay for
    // it on actual contact.
    return !WallBetween(enemy, player);
}
