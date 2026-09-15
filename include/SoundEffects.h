#ifndef SOUND_EFFECTS_H
#define SOUND_EFFECTS_H

#include "Sound.h"

// Sound effect constants
#define SCREW_SHOT_MUTE_FRAMES 5
#define ENEMY_HIT_MUTE_FRAMES 8
#define DOOR_OPEN_MUTE_FRAMES 10
#define COIN_COLLECT_MUTE_FRAMES 6
#define PLAYER_HIT_MUTE_FRAMES 10
#define BOSS_MOVE_MUTE_FRAMES 3
#define BOSS_SHOOT_MUTE_FRAMES 6
#define BOSS_SWORD_MUTE_FRAMES 8
#define BOSS_SPREAD_SHOT_MUTE_FRAMES 6
#define BOSS_DAMAGE_MUTE_FRAMES 10
#define ARENA_MUSIC_MUTE_FRAMES 4 // short — this fires every music step (see the sequencer below)

// Door opening melody constants
#define DOOR_MELODY_NOTE_DURATION 8  // Frames per note
#define DOOR_MELODY_TOTAL_NOTES 4    // Number of notes in the melody

// Enemy hit melody constants
#define ENEMY_HIT_MELODY_NOTE_DURATION 6  // Shorter duration for impact feel
#define ENEMY_HIT_MELODY_TOTAL_NOTES 3    // Fewer notes for quick impact

// Boss-run entrance fanfare constants (plays once entering the auto-scroll level)
#define BOSS_ENTRANCE_MELODY_NOTE_DURATION 8  // Same tempo as the door melody — readable notes, not a rapid chirp
#define BOSS_ENTRANCE_MELODY_TOTAL_NOTES 4

// Boss defeat melody constants (plays once per Boss death/split in StateBossFight)
#define BOSS_DEFEAT_MELODY_NOTE_DURATION 8
#define BOSS_DEFEAT_MELODY_TOTAL_NOTES 3

// Arena background music (custom step sequencer — see SoundEffects.c header
// comment on the generic engine both StateBossRun and StateBossFight use, and
// BOSSRUN_MUSIC_PATTERN_LEN/BOSSFIGHT_MUSIC_PATTERN_LEN there for pattern
// length — the two songs don't share one, so there's no public constant here).
// Not composed in a tracker: hUGETracker's .uge project format is a complex
// binary format (instruments/waves/patterns) with no editor available here —
// this is a from-scratch alternative built the same way the rest of this
// file's sounds are, out of raw PSG register writes, just repeating on a loop.

// Sound effect function declarations
// These live in an auto-assigned ROM bank (bank 255 / bankpack) and are called
// from code in other banks, so they must use the BANKED calling convention.
// Without BANKED, callers emit a direct call that only works when the caller
// happens to share the same bank, which breaks as soon as bankpack relocates them.
void PlayScrewShotSound(void) BANKED;
void PlayEnemyHitSound(void) BANKED;
void PlayDoorOpenSound(void) BANKED;
void PlayCoinCollectSound(void) BANKED;
void PlayPlayerHitSound(void) BANKED;
void PlayDoorOpeningMelody(void) BANKED;
void UpdateDoorOpeningMelody(void) BANKED;
void PlayEnemyHitMelody(void) BANKED;
void UpdateEnemyHitMelody(void) BANKED;

// Boss-run entrance fanfare — one-shot "get ready" hype cue for entering the auto-scroll level
void PlayBossEntranceMelody(void) BANKED;
void UpdateBossEntranceMelody(void) BANKED;

// Final-boss (StateBossFight) action sounds — one distinct cue per boss movement/action
void PlayBossMoveSound(void) BANKED;   // Boss.c: ChaseStep, each time it actually steps
void PlayBossShootSound(void) BANKED;  // Boss.c: FireAtPlayer, the single aimed shot
void PlayBossSpreadShotSound(void) BANKED; // Boss.c: FireAtPlayer, BOSS_VARIANT_TRIPLE_SHOT's 2 diagonal shots
void PlayBossSwordSound(void) BANKED;  // Boss.c: sword swing becomes active
void PlayBossDefeatMelody(void) BANKED; // Boss.c: HandleDeath (main boss splitting or a split copy dying)

// Player-takes-damage cue specific to the boss run / boss fight (distinct from
// the normal room game's PlayPlayerHitSound, same theme as every other boss
// sound getting its own identity instead of reusing the room-game's).
void PlayBossArenaDamageSound(void) BANKED;

// Arena background music — a tiny looping step sequencer (bass/lead/drums on
// channels 1/2/4), NOT hUGETracker music — see the ARENA_MUSIC_PATTERN_LEN
// comment above for why. One pair of Start/Update per level; call Start once
// from that state's START() and Update every frame from its UPDATE().
void PlayBossRunMusicStart(void) BANKED;
void PlayBossRunMusicUpdate(void) BANKED;
void PlayBossFightMusicStart(void) BANKED;
void PlayBossFightMusicUpdate(void) BANKED;

#endif // SOUND_EFFECTS_H
