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

// Door
#define CD_DOOR_STATE 0 // 0 = closed, 1 = open
#define CD_DOOR_COST 1 // Cost in Ready Coins to open the door

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

#endif
