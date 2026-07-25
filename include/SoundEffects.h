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
// These live in an auto-assigned ROM bank (bank 255 / bankpack) and are called
// from code in other banks, so they must use the BANKED calling convention.
// Without BANKED, callers emit a direct call that only works when the caller
// happens to share the same bank, which breaks as soon as bankpack relocates them.
void PlayScrewShotSound(void) BANKED;
void PlayEnemyHitSound(void) BANKED;
void PlayDoorOpenSound(void) BANKED;
void PlayCoinCollectSound(void) BANKED;
void PlayDoorOpeningMelody(void) BANKED;
void UpdateDoorOpeningMelody(void) BANKED;
void PlayEnemyHitMelody(void) BANKED;
void UpdateEnemyHitMelody(void) BANKED;

#endif // SOUND_EFFECTS_H
