#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteData.h"
#include "Hurtbox.h"

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
    return px < ex + e.w && ex < px + PLAYER_BODY_W &&
           py < ey + e.h && ey < py + PLAYER_BODY_H;
}
