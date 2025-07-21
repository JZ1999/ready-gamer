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
#define DOOR_TYPE 3

// Common custom_data indices

// Projectiles
#define CD_DIR           0

// Enemies
#define CD_ENEMY_HEALTH  0
#define CD_BLINK_TIMER   2
#define CD_MOVE_TIMER    3

// Door
#define CD_DOOR_STATE 0 // 0 = closed, 1 = open
#define CD_DOOR_COST 1 // Cost in Ready Coins to open the door

#define IsEnemyType(type) ((type) == BasicVirus || (type) == SpeedVirus || (type) == TankVirus)
#define IsDoorType(type) ((type) == Door)

#endif
