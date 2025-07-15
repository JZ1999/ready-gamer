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

// Common custom_data indices
#define CD_DIR           0
#define CD_ENEMY_HEALTH  0
#define CD_BLINK_TIMER 2

#endif
