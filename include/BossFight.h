#ifndef BOSS_FIGHT_H
#define BOSS_FIGHT_H

#include <gbdk/platform.h>
#include "Sprite.h"

/* mapbossarena.gbm dimensions — a single static screen (20x18 tiles =
 * 160x144px = SCREENWIDTH x SCREENHEIGHT exactly), so scroll_x/scroll_y
 * never need to move once StateBossFight starts. Same tile-id convention
 * as mapboss (0 = floor, 1 = wall) and the same shared tiles.gbr, cloned
 * from mapboss.gbm — see SESSION_NOTES.md for how it was generated. */
#define BOSSFIGHT_MAP_TILES_W 20
#define BOSSFIGHT_MAP_TILES_H 18
#define BOSSFIGHT_MAP_PIXELS_W (BOSSFIGHT_MAP_TILES_W * 8)
#define BOSSFIGHT_MAP_PIXELS_H (BOSSFIGHT_MAP_TILES_H * 8)

/* Shared with Boss.c (its own custom_data layout/state IDs) so
 * BossBeginSwordAttack — now living in BossFightCollision.c, see that
 * file's header comment for why — can set them without a private copy
 * silently drifting out of sync. */
#define CD_BOSS_STATE           1
#define CD_BOSS_TIMER           2
#define STATE_SWORD_TELEGRAPH   2
#define SWORD_TELEGRAPH_FRAMES  24
/* CD_BOSS_VARIANT — also shared with BossBulletAimed.c, which reads its
 * firing boss's variant to decide bullet speed. 0 = normal (unsplit main
 * boss). Phase-2 split copies get 1 or 2, see Boss.c's HandleDeath. */
#define CD_BOSS_VARIANT         6
#define BOSS_VARIANT_NORMAL     0
#define BOSS_VARIANT_FAST_BULLET 1 // faster bullets, slower own movement
#define BOSS_VARIANT_TRIPLE_SHOT 2 // fires 3 bullets (1 aimed + 2 diagonal) instead of 1

void InitBossFightScroll(void) BANKED;
UINT8 BossFightTranslateSprite(Sprite* sprite, INT8 dx, INT8 dy) BANKED;
void BossFightTakeDamage(Sprite* player) BANKED;
void BossBeginSwordAttack(Sprite* boss, UINT16 x, UINT16 y) BANKED;

#endif
