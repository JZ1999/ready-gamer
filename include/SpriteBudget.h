#ifndef SPRITE_BUDGET_H
#define SPRITE_BUDGET_H

#include <stddef.h>
#include "SpriteManager.h"

/*
 * ZGB's sprite pool holds N_SPRITE_MANAGER_SPRITES (20) sprites and
 * SpriteManagerAdd never checks if it is empty: adding a 21st sprite pops
 * garbage off the pool and corrupts memory (invalid-opcode crash).
 *
 * These macros refuse the add and return NULL instead. Macros (not functions)
 * so they can be used from any ROM bank without BANKED plumbing.
 *
 * sprite_manager_updatables[0] is the live sprite count. Sprites already
 * marked for removal still count until end of frame, which is correct: their
 * slot is not back in the pool yet.
 */
#define POOL_USED() (sprite_manager_updatables[0])

/* Slots kept free for the player's shots: enemies/bombs/markers use the
 * "Low" limit so they can never starve the player of the ability to shoot. */
#define POOL_RESERVED_FOR_SHOTS 2

#define POOL_HAS_ROOM()     (POOL_USED() < N_SPRITE_MANAGER_SPRITES)
#define POOL_HAS_ROOM_LOW() (POOL_USED() < (N_SPRITE_MANAGER_SPRITES - POOL_RESERVED_FOR_SHOTS))

/* Full pool limit: player shots, bomb dropped on Bomber death, doors. */
#define SafeSpriteAdd(type, x, y) \
    (POOL_HAS_ROOM() ? SpriteManagerAdd((type), (x), (y)) : NULL)
#define SafeSpriteAddEx(type, x, y, param) \
    (POOL_HAS_ROOM() ? SpriteManagerAddEx((type), (x), (y), (param)) : NULL)

/* Low limit: enemies, periodic bombs, spawn markers. */
#define SafeSpriteAddLow(type, x, y) \
    (POOL_HAS_ROOM_LOW() ? SpriteManagerAdd((type), (x), (y)) : NULL)

#endif
