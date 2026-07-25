#include "ZGBMain.h"
#include "Math.h"
#include "BankManager.h"
#include "Rooms.h"
#include "Scroll.h"
#include "Sprite.h"
#include "main.h"
#include <gb/gb.h>
#include <string.h>

IMPORT_MAP(map);
IMPORT_MAP(map2);
IMPORT_MAP(map3);
IMPORT_MAP(map4);
IMPORT_MAP(map5);

extern const unsigned char map_map[];
extern const unsigned char map2_map[];
extern const unsigned char map3_map[];
extern const unsigned char map4_map[];
extern const unsigned char map5_map[];

extern const void __bank_Rooms;
extern UINT8 last_tile_loaded;
extern UINT8 last_bg_pal_loaded;

#define MAP_TILE_W 40
#define SCROLL_TILE_REFRESH_H 22
#define SCROLL_PAD_LEFT 1
#define SCROLL_PAD_TOP 1

static const UINT8 scroll_collision_tiles[] = { 1, 0 };

#define TILE_FULL_BRICK 1
#define TILE_PARTIAL_BRICK_1 2
#define TILE_PARTIAL_BRICK_2 3

UINT8 next_state = StateMenu;

static void BindScrollMap(UINT8 room_index) {
	if (room_index == 4) {
		scroll_bank = BANK(map5);
		scroll_map = map5_map;
	} else if (room_index == 3) {
		scroll_bank = BANK(map4);
		scroll_map = map4_map;
	} else if (room_index == 2) {
		scroll_bank = BANK(map3);
		scroll_map = map3_map;
	} else if (room_index == 1) {
		scroll_bank = BANK(map2);
		scroll_map = map2_map;
	} else {
		scroll_bank = BANK(map);
		scroll_map = map_map;
	}
}

void EnsureRoomScrollMap(void) {
	BindScrollMap(current_room);
}

static UINT8 NormalizeCollisionTile(UINT8 tile) {
	if (current_state == StateGame) {
		if (U_LESS_THAN(255 - (UINT16)tile, N_SPRITE_TYPES)) {
			return 0;
		}
	}

	if (tile == TILE_FULL_BRICK || tile == TILE_PARTIAL_BRICK_1 || tile == TILE_PARTIAL_BRICK_2) {
		return tile;
	}

	return 0;
}

UINT8 GetRoomTileFromTable(UINT16 x, UINT16 y) {
	UINT8 tile;
	UINT16 index;

	if (x >= MAP_TILE_W || y >= 18) {
		return 0;
	}

	index = (UINT16)(y * MAP_TILE_W + x);

	if (current_room == 4) {
		PUSH_BANK(BANK(map5));
		tile = map5_map[index];
		POP_BANK;
	} else if (current_room == 3) {
		PUSH_BANK(BANK(map4));
		tile = map4_map[index];
		POP_BANK;
	} else if (current_room == 2) {
		PUSH_BANK(BANK(map3));
		tile = map3_map[index];
		POP_BANK;
	} else if (current_room == 1) {
		PUSH_BANK(BANK(map2));
		tile = map2_map[index];
		POP_BANK;
	} else {
		PUSH_BANK(BANK(map));
		tile = map_map[index];
		POP_BANK;
	}

	return NormalizeCollisionTile(tile);
}

static UINT8 CheckPartialBrickAt(UINT8 tile_x, UINT8 tile_y, UINT8 py, UINT8 coll_h, INT8 dy) {
	UINT8 tile;
	UINT8 sprite_bottom_y;
	UINT8 tile_top_y;
	UINT8 tile_quarter_y;
	UINT8 tile_three_quarter_y;

	if (tile_x >= scroll_tiles_w || tile_y >= scroll_tiles_h) {
		return 0;
	}

	tile = GetRoomTileFromTable(tile_x, tile_y);

	if (tile != TILE_PARTIAL_BRICK_1 && tile != TILE_PARTIAL_BRICK_2) {
		return 0;
	}

	sprite_bottom_y = (UINT8)(py + coll_h - 1 + dy);
	tile_top_y = tile_y * 8;
	tile_quarter_y = tile_top_y + 2;
	tile_three_quarter_y = tile_top_y + 6;

	if (tile == TILE_PARTIAL_BRICK_1) {
		if (sprite_bottom_y <= tile_quarter_y) {
			return 1;
		}
	} else if (sprite_bottom_y >= tile_three_quarter_y) {
		return 1;
	}

	return 0;
}

static UINT8 CheckEdgeMapCollision(UINT16 px, UINT16 py, UINT8 coll_w, UINT8 coll_h, INT8 dx, INT8 dy) {
	UINT8 tile_x;
	UINT8 tile_y;
	UINT8 row;
	UINT8 col;
	UINT8 end_col;
	UINT8 feet_row;
	INT16 nx;
	INT16 ny;
	INT16 pivot;

	nx = (INT16)px + dx;
	ny = (INT16)py + dy;

	if (U_LESS_THAN(nx, 0) || (UINT16)nx >= scroll_w ||
	    U_LESS_THAN(ny, 0) || (UINT16)ny >= scroll_h) {
		return 1;
	}

	if ((UINT16)(nx + coll_w - 1) >= scroll_w ||
	    (UINT16)(ny + coll_h - 1) >= scroll_h) {
		return 1;
	}

	feet_row = (UINT8)((ny + coll_h - 1) >> 3);

	if (dx) {
		if (dx > 0) {
			pivot = (INT16)(nx + coll_w - 1);
		} else {
			pivot = nx;
		}

		tile_x = (UINT8)(pivot >> 3);
		row = (UINT8)(ny >> 3);

		while (row <= feet_row) {
			if (GetRoomTileFromTable(tile_x, row) == TILE_FULL_BRICK) {
				return 1;
			}
			row++;
		}

		if (CheckPartialBrickAt(tile_x, feet_row, (UINT8)ny, coll_h, dy)) {
			return 1;
		}
	}

	if (dy) {
		if (dy > 0) {
			pivot = (INT16)(ny + coll_h - 1);
		} else {
			pivot = ny;
		}

		tile_y = (UINT8)(pivot >> 3);
		col = (UINT8)(nx >> 3);
		end_col = (UINT8)((nx + coll_w - 1) >> 3);

		while (col <= end_col) {
			if (GetRoomTileFromTable(col, tile_y) == TILE_FULL_BRICK) {
				return 1;
			}
			if (CheckPartialBrickAt(col, tile_y, (UINT8)ny, coll_h, dy)) {
				return 1;
			}
			col++;
		}
	}

	return 0;
}

UINT8 SafeTranslateSprite(Sprite* sprite, INT8 x, INT8 y) {
	UINT16 px;
	UINT16 py;
	UINT8 coll_w;
	UINT8 coll_h;
	INT16 nx;
	INT16 ny;

	px = sprite->x;
	py = sprite->y;
	coll_w = sprite->coll_w;
	coll_h = sprite->coll_h;

	if (x || y) {
		if (CheckEdgeMapCollision(px, py, coll_w, coll_h, x, y)) {
			return 1;
		}
	}

	if (x) {
		nx = (INT16)px + x;
		sprite->x = (UINT16)nx;
	}

	if (y) {
		ny = (INT16)py + y;
		sprite->y = (UINT16)ny;
	}

	return 0;
}

UINT8 EnemyMoveWithWallAvoidance(Sprite* enemy, INT16 dx, INT16 dy) {
	UINT8 result;
	INT16 avoid_dy;
	UINT16 enemy_y;
	UINT16 target_y;

	enemy_y = enemy->y;
	result = SafeTranslateSprite(enemy, (INT8)dx, (INT8)dy);

	if (result != 0 && scroll_target) {
		target_y = scroll_target->y;

		if (target_y < enemy_y) {
			avoid_dy = -1;
		} else if (target_y > enemy_y) {
			avoid_dy = 1;
		} else {
			avoid_dy = -1;
		}

		result = SafeTranslateSprite(enemy, 0, (INT8)avoid_dy);

		if (result != 0) {
			avoid_dy = (avoid_dy == -1) ? 1 : -1;
			result = SafeTranslateSprite(enemy, 0, (INT8)avoid_dy);
		}
	}

	return result;
}

void InitRoomScrollFromTable(UINT8 room_index) {
	if (room_index == 4) {
		InitScroll(BANK(map5), &map5, scroll_collision_tiles, 0);
	} else if (room_index == 3) {
		InitScroll(BANK(map4), &map4, scroll_collision_tiles, 0);
	} else if (room_index == 2) {
		InitScroll(BANK(map3), &map3, scroll_collision_tiles, 0);
	} else if (room_index == 1) {
		InitScroll(BANK(map2), &map2, scroll_collision_tiles, 0);
	} else {
		InitScroll(BANK(map), &map, scroll_collision_tiles, 0);
	}
	BindScrollMap(room_index);
}

void FinalizeRoomScrollFromTable(UINT8 room_index) {
	if (!scroll_target) {
		return;
	}

	if (room_index == 4) {
		ScrollSetMap(BANK(map5), &map5);
	} else if (room_index == 3) {
		ScrollSetMap(BANK(map4), &map4);
	} else if (room_index == 2) {
		ScrollSetMap(BANK(map3), &map3);
	} else if (room_index == 1) {
		ScrollSetMap(BANK(map2), &map2);
	} else {
		ScrollSetMap(BANK(map), &map);
	}

	BindScrollMap(room_index);
	CenterScrollOnPlayerFromTable();
}

void LoadRoomFromTable(UINT8 room_index) {
	last_tile_loaded = 0;
	last_bg_pal_loaded = 0;
	scroll_target = NULL;
	scroll_offset_x = 0;
	scroll_offset_y = 0;

	PUSH_BANK((UINT8)(UINT16)&__bank_Rooms);
	current_room = room_index;
	POP_BANK;

	InitRoomScrollFromTable(room_index);

	PUSH_BANK((UINT8)(UINT16)&__bank_Rooms);
	SpawnRoomEntities(room_index);
	POP_BANK;

	EnsureRoomScrollMap();
}

void SpawnRoomFromTable(UINT8 room_index) {
	PUSH_BANK((UINT8)(UINT16)&__bank_Rooms);
	SpawnRoomEntities(room_index);
	POP_BANK;
}

void GetRandomSpawnPositionFromTable(UINT8* x, UINT8* y) {
	PUSH_BANK((UINT8)(UINT16)&__bank_Rooms);
	GetRandomSpawnPosition(x, y);
	POP_BANK;
}

void EnsureRoomSpawnPointsFromTable(void) {
	PUSH_BANK((UINT8)(UINT16)&__bank_Rooms);
	EnsureRoomSpawnPoints();
	POP_BANK;
}

void CenterScrollOnPlayerFromTable(void) {
	UINT8 i;
	INT16 y;
	INT16 start_col;

	if (!scroll_target) {
		return;
	}

	BindScrollMap(current_room);

	start_col = (scroll_x >> 3) - SCROLL_PAD_LEFT;

	wait_vbl_done();
	PUSH_BANK(scroll_bank);
	y = scroll_y >> 3;
	for (i = 0; i != SCROLL_TILE_REFRESH_H && y != scroll_h; ++i, ++y) {
		ScrollUpdateRow(start_col, y - SCROLL_PAD_TOP);
	}
	POP_BANK;

	scroll_x_vblank = scroll_x;
	scroll_y_vblank = scroll_y;
	SCX_REG = scroll_x_vblank + (scroll_offset_x << 3);
	SCY_REG = scroll_y_vblank + (scroll_offset_y << 3);
}

UINT8 GetTileReplacement(UINT8* tile_ptr, UINT8* tile) {
	if(current_state == StateGame) {
		if(U_LESS_THAN(255 - (UINT16)*tile_ptr, N_SPRITE_TYPES)) {
			*tile = 0;
			return 255 - (UINT16)*tile_ptr;
		}

		*tile = *tile_ptr;
	}

	return 255u;
}
