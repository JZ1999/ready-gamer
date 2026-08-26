#ifndef BOSS_RUN_H
#define BOSS_RUN_H

#include <gbdk/platform.h>
#include "Sprite.h"

/* mapboss.gbm dimensions (see res/mapboss.gbm — generated, not hand-painted
 * in GBMB; see SESSION_NOTES.md for how/why). Tile 1 = wall, tile 0 = floor,
 * same convention as every other map in this project. */
#define BOSSRUN_MAP_TILES_W 240
#define BOSSRUN_MAP_TILES_H 18
#define BOSSRUN_MAP_PIXELS_W (BOSSRUN_MAP_TILES_W * 8)
#define BOSSRUN_MAP_PIXELS_H (BOSSRUN_MAP_TILES_H * 8)

void InitBossRunScroll(void);
UINT8 BossRunTileBlocked(UINT16 x, UINT16 y);
UINT8 BossRunTranslateSprite(Sprite* sprite, INT8 dx, INT8 dy);

#endif
