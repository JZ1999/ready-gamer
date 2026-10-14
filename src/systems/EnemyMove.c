#include "Banks/SetAutoBank.h"

#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "StateGame.h"

/*
 * Enemy movement step that also treats CLOSED DOORS as walls.
 *
 * Doors are sprites, not map tiles, so map collision alone (SafeTranslateSprite)
 * let enemies walk straight through them. They then got stuck in areas the
 * player couldn't enter yet, and since a wave only ends when every enemy is
 * dead, the room softlocked (found by the map4 e2e test). Lives in its own
 * auto-banked file because ROM bank 0 (ZGBMain.c) is full.
 */
UINT8 EnemyTranslateSprite(Sprite* enemy, INT8 dx, INT8 dy) BANKED {
	UINT8 i;
	Sprite* spr;
	INT16 nx = (INT16)enemy->x + dx;
	INT16 ny = (INT16)enemy->y + dy;
	SPRITEMANAGER_ITERATE(i, spr) {
		if (spr->type != Door || spr->custom_data[CD_DOOR_STATE] != 0) continue;
		if (nx < (INT16)(spr->x + spr->coll_w) && nx + (INT16)enemy->coll_w > (INT16)spr->x &&
		    ny < (INT16)(spr->y + spr->coll_h) && ny + (INT16)enemy->coll_h > (INT16)spr->y) {
			return 1;
		}
	}
	return SafeTranslateSprite(enemy, dx, dy);
}
