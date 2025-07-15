#ifndef ZGBMAIN_H
#define ZGBMAIN_H

#define STATES \
_STATE(StateGame)\
STATE_DEF_END

#define SPRITES \
_SPRITE_DMG(SpritePlayer, player)\
_SPRITE_DMG(SpriteScrew, screw)\
_SPRITE_DMG(BasicVirus, basicVirus)\
SPRITE_DEF_END

#include "ZGBMain_Init.h"

#endif