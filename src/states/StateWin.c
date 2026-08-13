#include "Banks/SetAutoBank.h"
#include "main.h"

#include <gb/gb.h>
#include "ZGBMain.h"
#include "Scroll.h"
#include "Keys.h"
#include "Music.h"
#include "SpriteManager.h"
#include "Print.h"

IMPORT_MAP(GameOvermap);
IMPORT_TILES(MenuTileset);
IMPORT_TILES(font);

DECLARE_MUSIC(menu);

extern INT8 scroll_h_border;

/* Raffle code for Game Boy Advance SP sorteo — A-Z/0-9 only for the GB font. */
#define WIN_CODE "RGGBA26"

void START() {
	HIDE_WIN;
	SetWindowY(144);
	scroll_h_border = 0;
	scroll_offset_x = 0;
	scroll_offset_y = 0;
	scroll_target = 0;

	SpriteManagerReset();

	InitScrollWithTiles(BANK(GameOvermap), &GameOvermap, BANK(MenuTileset), &MenuTileset, 0, 0);

	INIT_FONT(font, PRINT_BKG);
	PRINT(6, 4, "YOU WIN!");
	PRINT(2, 7, "SORTEO GBA SP");
	PRINT(4, 9, WIN_CODE);
	PRINT(6, 12, "PRESS A");

	SHOW_BKG;
	HIDE_SPRITES;

	PlayMusic(menu, 0);
}

void UPDATE() {
	if(KEY_TICKED(J_START) || KEY_TICKED(J_A) || KEY_TICKED(J_B)) {
		SetState(StateMenu);
	}
}
