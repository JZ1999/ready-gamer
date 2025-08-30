#ifndef ZGBMAIN_H
#define ZGBMAIN_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#define STATES \
_STATE(StateGame)\
STATE_DEF_END

#define SPRITES \
_SPRITE_DMG(SpritePlayer, player)\
_SPRITE_DMG(SpriteScrew, screw)\
_SPRITE_DMG(BasicVirus, basicVirus)\
_SPRITE_DMG(SpeedVirus, speedVirus)\
_SPRITE_DMG(TankVirus, tankVirus)\
_SPRITE_DMG(Door, door)\
SPRITE_DEF_END

#include "ZGBMain_Init.h"

#endif