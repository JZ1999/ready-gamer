#pragma bank 255

#include "Banks/SetAutoBank.h"

#include <stdint.h>
#include <gbdk/platform.h>
#include <gbdk/metasprites.h>

/* Named bossGfx.c so it does not collide with sprites/Boss.c on
 * case-insensitive Windows builds, same as chargeVirusGfx.c. Symbol
 * stays `boss`. 16x16: 2 columns x a single row of 8x16 hardware sprites
 * (SPRITES_8x16 is this project's global sprite size), same footprint as
 * the player and every enemy. Single row (dy always 0) on purpose — a
 * first attempt at a 2-row (dy=16, 24x32) layout rendered as a scattered
 * diagonal staircase in BGB (real hardware timing) even though the tile
 * data decoded correctly offline, so this stays inside the only pattern
 * this project has proven safe.
 *
 * "Face creature" design, traced pixel-for-pixel from the user's reference
 * image (not hand-drawn): edge-detected the reference PNG's native pixel
 * grid (~23x23, flat 4-color art, no antialiasing) and downsampled it to
 * 16x16 by majority vote per cell. 3 horns (center pair notched), side
 * fins, diagonal cream/dark-red split, dark eyes, jagged mouth.
 * Previous (pre-trace) "arcade cabinet monster" design backed up at
 * bossGfx_arcade_backup.c.bak — restore by copying it back over this file
 * if the new art needs to be reverted.
 * Frame 0 = idle (dark eyes, cream highlight). Frame 1 = telegraph (eyes
 * flash bright), used for the shoot/sword windup, same idea as
 * ChargeVirus's charged frame. */

BANKREF(boss)

const palette_color_t boss_palettes[4] = {
	RGB8(0, 0, 0), RGB8(242, 214, 163), RGB8(194, 73, 63), RGB8(90, 26, 42)
};

const uint8_t boss_tiles[128] = {
0x00,0x00,0x0d,0x0d,0x0c,0x0c,0x0c,0x0c,0x01,0x00,0x0f,0x0c,0x1f,0x18,0x5f,0x5e,
0xdf,0xd2,0x44,0x5f,0x40,0x5f,0x00,0x1f,0x15,0x1f,0x1c,0x1f,0x0c,0x0f,0x01,0x01,
0x80,0x80,0xd8,0xd8,0x38,0x38,0x38,0x38,0x20,0xe0,0x18,0xf8,0x08,0xf8,0x7e,0xfe,
0x7f,0xc7,0x26,0xfe,0x06,0xfe,0x04,0xfc,0x58,0xf8,0x38,0xf8,0x18,0xf8,0xc0,0xc0,
0x00,0x00,0x0d,0x0d,0x0c,0x0c,0x0c,0x0c,0x01,0x00,0x0f,0x0c,0x1f,0x18,0x5f,0x40,
0xdf,0xc0,0x44,0x5f,0x40,0x5f,0x00,0x1f,0x15,0x1f,0x1c,0x1f,0x0c,0x0f,0x01,0x01,
0x80,0x80,0xd8,0xd8,0x38,0x38,0x38,0x38,0x20,0xe0,0x18,0xf8,0x08,0xf8,0x7e,0x82,
0x7f,0x83,0x26,0xfe,0x06,0xfe,0x04,0xfc,0x58,0xf8,0x38,0xf8,0x18,0xf8,0xc0,0xc0
};

const metasprite_t boss_metasprite0[] = {
	METASPR_ITEM(0, 0, 0, S_PAL(0)),
	METASPR_ITEM(0, 8, 2, S_PAL(0)),
	METASPR_TERM
};

const metasprite_t boss_metasprite1[] = {
	METASPR_ITEM(0, 0, 4, S_PAL(0)),
	METASPR_ITEM(0, 8, 6, S_PAL(0)),
	METASPR_TERM
};

const metasprite_t* const boss_metasprites[2] = {
	boss_metasprite0,
	boss_metasprite1
};

#include "MetaSpriteInfo.h"
const struct MetaSpriteInfo boss = {
	16, /* width */
	16, /* height */
	8, /* num tiles */
	boss_tiles,
	1, /* num palettes */
	boss_palettes,
	2, /* num sprites / frames */
	boss_metasprites,
};
