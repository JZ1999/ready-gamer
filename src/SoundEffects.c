#include "Banks/SetAutoBank.h"
#include "SoundEffects.h"

// Door opening melody state
static UINT8 door_melody_timer = 0;
static UINT8 door_melody_note_index = 0;
static UINT8 door_melody_active = 0;

// Enemy hit melody state
static UINT8 enemy_hit_melody_timer = 0;
static UINT8 enemy_hit_melody_note_index = 0;
static UINT8 enemy_hit_melody_active = 0;

/**
 * Plays the screw shot sound effect
 * Uses Channel 1 with a short, sharp sound appropriate for projectile firing
 */
void PlayScrewShotSound(void) {
    // Channel 1: Square wave with envelope for a sharp "pew" sound
    // NR10: Sweep (disabled)
    // NR11: Length and duty cycle (50% duty, short length)
    // NR12: Volume envelope (start loud, fade quickly)
    // NR13: Frequency low byte
    // NR14: Frequency high byte and control
    
    PlayFx(CHANNEL_1, SCREW_SHOT_MUTE_FRAMES, 
           0x00,   // NR10: No sweep
           0x42,   // NR11: 50% duty, length 2
           0x73,   // NR12: Volume 7, envelope down, step 3
           0x86,   // NR13: Frequency low (high pitch)
           0x87);  // NR14: Frequency high, restart sound
}

/**
 * Plays the enemy hit sound effect
 * Uses the new low-frequency melody system for better impact feel
 */
void PlayEnemyHitSound(void) {
    PlayEnemyHitMelody(); // Use the new melody system
}

/**
 * Plays a single note of the door opening melody
 * Uses different channels for variety and harmony
 */
static void PlayDoorMelodyNote(UINT8 note_index) {
    switch(note_index) {
        case 0: // First note: Ascending tone (like a door unlocking)
            PlayFx(CHANNEL_1, DOOR_MELODY_NOTE_DURATION,
                   0x00,   // NR10: No sweep
                   0x42,   // NR11: 50% duty, length 2
                   0x73,   // NR12: Volume 7, envelope down, step 3
                   0x73,   // NR13: Frequency low (medium-high pitch)
                   0x87);  // NR14: Frequency high, restart sound
            break;
            
        case 1: // Second note: Higher tone (door opening)
            PlayFx(CHANNEL_2, DOOR_MELODY_NOTE_DURATION,
                   0x00,   // NR21: 50% duty, length 3
                   0x83,   // NR22: Volume 8, envelope down, step 3
                   0x86,   // NR23: Frequency low (higher pitch)
                   0x87);  // NR24: Frequency high, restart sound
            break;
            
        case 2: // Third note: Harmonizing tone (door fully open)
            PlayFx(CHANNEL_3, DOOR_MELODY_NOTE_DURATION,
                   0x80,   // NR30: Wave channel on
                   0x00,   // NR31: Length (max)
                   0x20,   // NR32: Volume 50%
                   0x96,   // NR33: Frequency low (highest pitch)
                   0x87);  // NR34: Frequency high, restart sound
            break;
            
        case 3: // Fourth note: Final triumphant tone (success!)
            PlayFx(CHANNEL_1, DOOR_MELODY_NOTE_DURATION,
                   0x00,   // NR10: No sweep
                   0x42,   // NR11: 50% duty, length 2
                   0x73,   // NR12: Volume 7, envelope down, step 3
                   0xA6,   // NR13: Frequency low (highest pitch for finale)
                   0x87);  // NR14: Frequency high, restart sound
            break;
    }
}

/**
 * Starts the door opening melody sequence
 */
void PlayDoorOpeningMelody(void) {
    door_melody_active = 1;
    door_melody_timer = DOOR_MELODY_NOTE_DURATION;
    door_melody_note_index = 0;
    PlayDoorMelodyNote(0); // Play first note immediately
}

/**
 * Updates the door opening melody (call this in UPDATE function)
 */
void UpdateDoorOpeningMelody(void) {
    if (!door_melody_active) return;
    
    if (--door_melody_timer == 0) {
        door_melody_note_index++;
        if (door_melody_note_index >= DOOR_MELODY_TOTAL_NOTES) {
            door_melody_active = 0; // Melody complete
            return;
        }
        
        door_melody_timer = DOOR_MELODY_NOTE_DURATION;
        PlayDoorMelodyNote(door_melody_note_index);
    }
}

/**
 * Legacy door opening sound (kept for compatibility)
 */
void PlayDoorOpenSound(void) {
    PlayDoorOpeningMelody(); // Use the new melody system
}

/**
 * Plays the coin collection sound effect
 * Uses Channel 4 (noise) for a distinctive coin sound
 */
void PlayCoinCollectSound(void) {
    // Channel 4: Noise channel for a distinctive coin sound
    PlayFx(CHANNEL_4, COIN_COLLECT_MUTE_FRAMES,
           0x00,   // NR41: Length
           0x00,   // NR42: Volume envelope
           0x00,   // NR43: Frequency and randomness
           0x80);  // NR44: Restart sound
}

/**
 * Plays a single note of the enemy hit melody
 * Uses very low frequencies for impact feel
 */
static void PlayEnemyHitMelodyNote(UINT8 note_index) {
    switch(note_index) {
        case 0: // First note: Very low thud (impact start)
            PlayFx(CHANNEL_1, ENEMY_HIT_MELODY_NOTE_DURATION,
                   0x00,   // NR10: No sweep
                   0x42,   // NR11: 50% duty, length 2
                   0x73,   // NR12: Volume 7, envelope down, step 3
                   0x13,   // NR13: Frequency low (very low pitch)
                   0x87);  // NR14: Frequency high, restart sound
            break;
            
        case 1: // Second note: Slightly higher thud (impact middle)
            PlayFx(CHANNEL_2, ENEMY_HIT_MELODY_NOTE_DURATION,
                   0x00,   // NR21: 50% duty, length 3
                   0x83,   // NR22: Volume 8, envelope down, step 3
                   0x23,   // NR23: Frequency low (low pitch)
                   0x87);  // NR24: Frequency high, restart sound
            break;
            
        case 2: // Third note: Final low thud (impact end)
            PlayFx(CHANNEL_3, ENEMY_HIT_MELODY_NOTE_DURATION,
                   0x80,   // NR30: Wave channel on
                   0x00,   // NR31: Length (max)
                   0x20,   // NR32: Volume 50%
                   0x33,   // NR33: Frequency low (low pitch)
                   0x87);  // NR34: Frequency high, restart sound
            break;
    }
}

/**
 * Starts the enemy hit melody sequence
 */
void PlayEnemyHitMelody(void) {
    enemy_hit_melody_active = 1;
    enemy_hit_melody_timer = ENEMY_HIT_MELODY_NOTE_DURATION;
    enemy_hit_melody_note_index = 0;
    PlayEnemyHitMelodyNote(0); // Play first note immediately
}

/**
 * Updates the enemy hit melody (call this in UPDATE function)
 */
void UpdateEnemyHitMelody(void) {
    if (!enemy_hit_melody_active) return;
    
    if (--enemy_hit_melody_timer == 0) {
        enemy_hit_melody_note_index++;
        if (enemy_hit_melody_note_index >= ENEMY_HIT_MELODY_TOTAL_NOTES) {
            enemy_hit_melody_active = 0; // Melody complete
            return;
        }
        
        enemy_hit_melody_timer = ENEMY_HIT_MELODY_NOTE_DURATION;
        PlayEnemyHitMelodyNote(enemy_hit_melody_note_index);
    }
}
