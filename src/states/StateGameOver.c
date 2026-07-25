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

DECLARE_MUSIC(gameover);

extern INT8 scroll_h_border;

void START() {
	HIDE_WIN;
	SetWindowY(144);
	scroll_h_border = 0;
	scroll_offset_x = 0;
	scroll_offset_y = 0;
	scroll_target = 0;

	SpriteManagerReset();

	/* Frame border from MenuTileset (only shared tiles that still match). */
	InitScrollWithTiles(BANK(GameOvermap), &GameOvermap, BANK(MenuTileset), &MenuTileset, 0, 0);

	/* Text is drawn with the game font — old center art no longer exists in MenuTileset. */
	INIT_FONT(font, PRINT_BKG);
	PRINT(5, 7, "GAME OVER");
	PRINT(6, 10, "PRESS A");

	SHOW_BKG;
	HIDE_SPRITES;

	PlayMusic(gameover, 0);
}

void UPDATE() {
	if(KEY_TICKED(J_START) || KEY_TICKED(J_A) || KEY_TICKED(J_B)) {
		SetState(StateMenu);
	}
}
