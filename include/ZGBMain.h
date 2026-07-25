#ifndef ZGBMAIN_H
#define ZGBMAIN_H

#ifndef NULL
#define NULL ((void*)0)
#endif

#define STATES \
_STATE(StateMenu)\
_STATE(StateGame)\
_STATE(StateGameOver)\
STATE_DEF_END

#define SPRITES \
_SPRITE_DMG(SpritePlayer, player)\
_SPRITE_DMG(SpriteScrew, screw)\
_SPRITE_DMG(BasicVirus, basicVirus)\
_SPRITE_DMG(SpeedVirus, speedVirus)\
_SPRITE_DMG(TankVirus, tankVirus)\
_SPRITE_DMG(BomberVirus, bomberVirus)\
_SPRITE_DMG(ChargeVirus, chargeVirus)\
_SPRITE_DMG(Bomb, bomb)\
_SPRITE_DMG(Door, door)\
_SPRITE_DMG(SpawnPoint, spawner)\
_SPRITE_DMG(NextLevelPortal, spawner)\
_SPRITE_DMG(ElectricProjectile, screw)\
_SPRITE_DMG(ElectricityPickup, electricity)\
_SPRITE_DMG(CoinsPickup, coins)\
SPRITE_DEF_END

#include "ZGBMain_Init.h"

#endif