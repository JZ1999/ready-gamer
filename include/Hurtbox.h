#ifndef HURTBOX_H
#define HURTBOX_H

#include "Sprite.h"

// Contact damage based on drawn pixels instead of ZGB's coll boxes (which
// have no offset and are 8x8 for enemies). BANKED: called from sprite banks.

// 1 if the player's drawn body overlaps the enemy's drawn body by >= 1 px.
UINT8 PlayerTouchesEnemy(Sprite* player, Sprite* enemy) BANKED;

#endif
