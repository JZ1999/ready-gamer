#include "Banks/SetAutoBank.h"
#include "main.h"

#include <gb/gb.h>
#include "ZGBMain.h"
#include "Scroll.h"
#include "Keys.h"
#include "Music.h"
#include "Music.h"
#include "Print.h"

IMPORT_MAP(GameOvermap);
IMPORT_TILES(MenuTileset);

DECLARE_MUSIC(gameover);

void START() {
	INIT_BKG(GameOvermap);

  DPRINT_POS(0, 0);
  DPrintf("GAME OVER");

	PlayMusic(gameover, 0);
}

void UPDATE() {
	if(KEY_TICKED(J_START) || KEY_TICKED(J_A) || KEY_TICKED(J_B)) {
		SetState(StateMenu);
	}
}