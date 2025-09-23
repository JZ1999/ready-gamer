#ifndef SOUND_EFFECTS_H
#define SOUND_EFFECTS_H

#include "Sound.h"

// Sound effect constants
#define SCREW_SHOT_MUTE_FRAMES 5
#define ENEMY_HIT_MUTE_FRAMES 8
#define DOOR_OPEN_MUTE_FRAMES 10
#define COIN_COLLECT_MUTE_FRAMES 6

// Door opening melody constants
#define DOOR_MELODY_NOTE_DURATION 8  // Frames per note
#define DOOR_MELODY_TOTAL_NOTES 4    // Number of notes in the melody

// Enemy hit melody constants
#define ENEMY_HIT_MELODY_NOTE_DURATION 6  // Shorter duration for impact feel
#define ENEMY_HIT_MELODY_TOTAL_NOTES 3    // Fewer notes for quick impact

// Sound effect function declarations
void PlayScrewShotSound(void);
void PlayEnemyHitSound(void);
void PlayDoorOpenSound(void);
void PlayCoinCollectSound(void);
void PlayDoorOpeningMelody(void);
void UpdateDoorOpeningMelody(void);
void PlayEnemyHitMelody(void);
void UpdateEnemyHitMelody(void);

#endif // SOUND_EFFECTS_H
