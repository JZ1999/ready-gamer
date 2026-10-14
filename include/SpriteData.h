// SpriteData.h
#ifndef SPRITE_DATA_H
#define SPRITE_DATA_H

// Direction enum for all sprites
typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} Direction;

#define ENEMY_TYPE_BASIC 0
#define ENEMY_TYPE_SPEED 1
#define ENEMY_TYPE_TANK 2
#define ENEMY_TYPE_BOMBER 3
#define ENEMY_TYPE_CHARGE 4
#define DOOR_TYPE 5

// Common custom_data indices

// Projectiles
#define CD_DIR           0

// Enemies
#define CD_ENEMY_HEALTH  0
#define CD_BLINK_TIMER   2
#define CD_MOVE_TIMER    3
#define CD_BOMB_TIMER    4  // BomberVirus: low byte of frames until next bomb drop
#define CD_BOMB_TIMER_HI 5  // BomberVirus: high byte (16-bit timer, max ~65535 frames)

// Spawn point (portal enemies come out of)
#define CD_SPAWN_ANIM_TIMER 0 // frames until next animation frame
#define CD_SPAWN_WARNING    1 // frames left of the "enemy incoming" warning (0 = idle)

// Door
#define CD_DOOR_STATE 0 // 0 = closed, 1 = open
#define CD_DOOR_COST 1 // Cost in Ready Coins to open the door

// Door blocking box, matched to the visible pixels of door.gbr (x 5..12, full
// height). ZGB's coll_w/coll_h cover the whole 16x16 cell, which stopped the
// player ~7px before the drawn door. Offsets are relative to sprite x/y.
#define DOOR_HITBOX_X 5
#define DOOR_HITBOX_Y 0
#define DOOR_HITBOX_W 8
#define DOOR_HITBOX_H 16

// Player visible pixels in player.gbr (x 1..14, y 1..15 on every frame).
// Not used by door blocking anymore (doors now block like wall tiles); kept
// as measured reference for the pending damage-hurtbox decision.
#define PLAYER_BODY_X 1
#define PLAYER_BODY_Y 1
#define PLAYER_BODY_W 14
#define PLAYER_BODY_H 15

// Coin pickup
#define CD_COIN_VALUE 0 // Ready Coins granted on pickup, set from CoinPickupPlacement

// Player
#define CD_PLAYER_HEALTH     0
#define CD_INVINCIBILITY     1
#define CD_WALK_TIMER        2
#define CD_PLAYER_ELECTRIC   3

// Projectiles (SpriteScrew / ElectricProjectile)
#define CD_ANIM_TIMER    1
#define CD_ANIM_OFFSET   2
#define CD_LIFETIME      3

#define PROJECTILE_DAMAGE_NORMAL   1
#define PROJECTILE_DAMAGE_ELECTRIC 2

#define IsEnemyType(type) ((type) == BasicVirus || (type) == SpeedVirus || (type) == TankVirus || (type) == BomberVirus || (type) == ChargeVirus)
#define IsDoorType(type) ((type) == Door)

// Ready Coins paid per kill: tougher enemies pay more (Tank has 5 HP, Charge
// dashes) so the maze rooms' doors don't turn into a grind.
#define KillCoinValue(type) (((type) == TankVirus || (type) == ChargeVirus) ? 2 : 1)

#endif
