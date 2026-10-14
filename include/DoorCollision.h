#ifndef DOOR_COLLISION_H
#define DOOR_COLLISION_H

#include "Sprite.h"

// Closed-door blocking shared by the player and room enemies. A closed door
// is treated as wall tiles (its tile footprint), tested with the same inset
// wall box as map collision. BANKED: called from bank 0 (ZGBMain.c) and
// sprite banks.

// 1 if moving sprite by (dx, dy) would push its wall box into a closed door.
UINT8 SpriteBlockedByClosedDoor(Sprite* sprite, INT8 dx, INT8 dy) BANKED;

#endif
