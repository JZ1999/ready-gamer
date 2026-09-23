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

// Boss-run entrance fanfare state
static UINT8 boss_entrance_melody_timer = 0;
static UINT8 boss_entrance_melody_note_index = 0;
static UINT8 boss_entrance_melody_active = 0;

/**
 * Plays the screw shot sound effect
 * Uses Channel 1 with a short, sharp sound appropriate for projectile firing
 */
void PlayScrewShotSound(void) BANKED {
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
void PlayEnemyHitSound(void) BANKED {
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
void PlayDoorOpeningMelody(void) BANKED {
    door_melody_active = 1;
    door_melody_timer = DOOR_MELODY_NOTE_DURATION;
    door_melody_note_index = 0;
    PlayDoorMelodyNote(0); // Play first note immediately
}

/**
 * Updates the door opening melody (call this in UPDATE function)
 */
void UpdateDoorOpeningMelody(void) BANKED {
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
void PlayDoorOpenSound(void) BANKED {
    PlayDoorOpeningMelody(); // Use the new melody system
}

/**
 * Plays the coin collection sound effect
 * Uses Channel 4 (noise) for a distinctive coin sound
 */
void PlayCoinCollectSound(void) BANKED {
    // Channel 4: Noise channel for a distinctive coin sound
    PlayFx(CHANNEL_4, COIN_COLLECT_MUTE_FRAMES,
           0x00,   // NR41: Length
           0x00,   // NR42: Volume envelope
           0x00,   // NR43: Frequency and randomness
           0x80);  // NR44: Restart sound
}

/**
 * Plays the player-hit sound effect (life lost / damage taken)
 * Low descending square tone — deliberately harsher/lower than the enemy-hit
 * sound so a player hit reads as distinct from landing a hit on an enemy.
 */
void PlayPlayerHitSound(void) BANKED {
    PlayFx(CHANNEL_2, PLAYER_HIT_MUTE_FRAMES,
           0x00,   // NR21: 50% duty, length
           0x84,   // NR22: Volume 8, envelope down, step 4 (longer fade)
           0x10,   // NR23: Frequency low (low pitch)
           0x87);  // NR24: Frequency high, restart sound
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
void PlayEnemyHitMelody(void) BANKED {
    enemy_hit_melody_active = 1;
    enemy_hit_melody_timer = ENEMY_HIT_MELODY_NOTE_DURATION;
    enemy_hit_melody_note_index = 0;
    PlayEnemyHitMelodyNote(0); // Play first note immediately
}

/**
 * Updates the enemy hit melody (call this in UPDATE function)
 */
void UpdateEnemyHitMelody(void) BANKED {
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

/**
 * Plays a single note of the boss-run entrance fanfare.
 * Rising pitch across all 4 notes (unlike the door melody, which tops out
 * lower) — meant to read as "get ready", played once on StateBossRun entry.
 */
static void PlayBossEntranceMelodyNote(UINT8 note_index) {
    switch(note_index) {
        case 0:
            PlayFx(CHANNEL_1, BOSS_ENTRANCE_MELODY_NOTE_DURATION,
                   0x00,   // NR10: No sweep
                   0x42,   // NR11: 50% duty, length 2
                   0x73,   // NR12: Volume 7, envelope down, step 3
                   0x50,   // NR13: Frequency low
                   0x87);  // NR14: Frequency high, restart sound
            break;

        case 1:
            PlayFx(CHANNEL_2, BOSS_ENTRANCE_MELODY_NOTE_DURATION,
                   0x00,   // NR21: 50% duty, length
                   0x83,   // NR22: Volume 8, envelope down, step 3
                   0x70,   // NR23: Frequency low (higher than note 0)
                   0x87);  // NR24: Frequency high, restart sound
            break;

        case 2:
            // NR34 bit 6 set (0xC7, not the usual 0x87) — the wave channel has
            // no envelope, so with the length counter disabled (bit 6 clear,
            // what every other melody's channel-3 note uses) it rings forever
            // once triggered instead of fading. That's normally masked by the
            // next gameplay sound retriggering the channel a moment later, but
            // right after this fanfare there's often nothing else playing yet
            // — it was the actual source of the reported "shrill continuous
            // beep" (NR31 below gives it a real ~130ms length so it self-stops).
            PlayFx(CHANNEL_3, BOSS_ENTRANCE_MELODY_NOTE_DURATION,
                   0x80,   // NR30: Wave channel on
                   0xDE,   // NR31: Length ~130ms — long enough to be heard, short enough to actually stop
                   0x20,   // NR32: Volume 50%
                   0x90,   // NR33: Frequency low (higher still)
                   0xC7);  // NR34: Frequency high, length enabled, restart sound
            break;

        case 3: // Final note: highest pitch — the "go!" cue. Was 0xC0 (~2048Hz,
                // right at the top of this melody's usable range) — reported as
                // a shrill screech, pulled back to a still-bright but much less
                // piercing peak.
            PlayFx(CHANNEL_1, BOSS_ENTRANCE_MELODY_NOTE_DURATION,
                   0x00,   // NR10: No sweep
                   0x42,   // NR11: 50% duty, length 2
                   0x73,   // NR12: Volume 7, envelope down, step 3
                   0xA8,   // NR13: Frequency low (bright peak, not screeching)
                   0x87);  // NR14: Frequency high, restart sound
            break;
    }
}

/**
 * Starts the boss-run entrance fanfare — call once from StateBossRun's START().
 */
void PlayBossEntranceMelody(void) BANKED {
    boss_entrance_melody_active = 1;
    boss_entrance_melody_timer = BOSS_ENTRANCE_MELODY_NOTE_DURATION;
    boss_entrance_melody_note_index = 0;
    PlayBossEntranceMelodyNote(0);
}

/**
 * Advances the boss-run entrance fanfare — call every frame from StateBossRun's UPDATE().
 */
void UpdateBossEntranceMelody(void) BANKED {
    if (!boss_entrance_melody_active) return;

    if (--boss_entrance_melody_timer == 0) {
        boss_entrance_melody_note_index++;
        if (boss_entrance_melody_note_index >= BOSS_ENTRANCE_MELODY_TOTAL_NOTES) {
            boss_entrance_melody_active = 0;
            return;
        }

        boss_entrance_melody_timer = BOSS_ENTRANCE_MELODY_NOTE_DURATION;
        PlayBossEntranceMelodyNote(boss_entrance_melody_note_index);
    }
}

/**
 * Plays the final-boss movement-step sound (Boss.c's ChaseStep, each time it
 * actually moves). Deliberately quiet/short — this can fire every few frames
 * while the boss is chasing, so it needs to read as a texture, not a jingle.
 */
void PlayBossMoveSound(void) BANKED {
    // Noise channel instead of a tonal square note: a step/thud should be a
    // percussive texture, not another pitched "beep" alongside every other
    // square-wave SFX in the game. Also only called every 3rd actual step
    // now (see Boss.c's ChaseStep) — every step at 60fps was nonstop chirping.
    PlayFx(CHANNEL_4, BOSS_MOVE_MUTE_FRAMES,
           0x00,   // NR41: Length
           0x61,   // NR42: Volume 6, envelope down, step 1 (very fast decay — a tap, not a ring)
           0x56,   // NR43: Low clock shift/divisor — dull low thud, not a hiss
           0x80);  // NR44: Restart sound
}

/**
 * Plays the final-boss shoot sound (Boss.c's FireAtPlayer). Lower and
 * harsher than the player's own screw-shot sound, with a downward pitch
 * sweep (NR10) for a "growl" that reads as the boss's attack, not the
 * player's — deliberately distinct so a hit sound can't be mistaken for one
 * of your own shots mid-fight.
 */
void PlayBossShootSound(void) BANKED {
    // No pitch sweep this time — a sweeping square wave is the single most
    // generic "8-bit laser beep" there is; dropping it and leaning on a
    // wider duty (75%) + low pitch instead reads more like a heavy "thoom".
    PlayFx(CHANNEL_1, BOSS_SHOOT_MUTE_FRAMES,
           0x00,   // NR10: No sweep
           0xC2,   // NR11: 75% duty (fuller/rounder than the player's own 25%), length 2
           0x84,   // NR12: Volume 8, envelope down, step 4 (slower fade, heavier)
           0x00,   // NR13: Frequency low (lowest available — heavy, not a beep)
           0x87);  // NR14: Frequency high, restart sound
}

/**
 * Plays the final-boss sword-swing sound (Boss.c, when STATE_SWORD_ACTIVE
 * begins). Noise channel for a sharp "shing" instead of a tonal note.
 */
void PlayBossSwordSound(void) BANKED {
    // Width-mode bit set (7-bit LFSR) for a higher, more tonal/metallic
    // noise instead of full white-noise hiss — reads closer to "shing" than
    // the move-thud (also channel 4) does, so the two don't blur together.
    PlayFx(CHANNEL_4, BOSS_SWORD_MUTE_FRAMES,
           0x00,   // NR41: Length
           0x92,   // NR42: Volume 9, envelope down, step 2 (fast decay)
           0x4A,   // NR43: Clock shift 4, 7-bit width, divisor 2 — metallic ping
           0x80);  // NR44: Restart sound
}

/**
 * Plays a single note of the boss-defeat melody. Descending pitch (opposite
 * of the entrance fanfare) — meant to read as the boss cracking/splitting,
 * not a fanfare win jingle (StateWin already has its own music for that).
 */
static void PlayBossDefeatMelodyNote(UINT8 note_index) {
    switch(note_index) {
        case 0:
            PlayFx(CHANNEL_1, BOSS_DEFEAT_MELODY_NOTE_DURATION,
                   0x00,   // NR10: No sweep
                   0x42,   // NR11: 50% duty, length 2
                   0x73,   // NR12: Volume 7, envelope down, step 3
                   0x60,   // NR13: Frequency low
                   0x87);  // NR14: Frequency high, restart sound
            break;

        case 1:
            PlayFx(CHANNEL_2, BOSS_DEFEAT_MELODY_NOTE_DURATION,
                   0x00,   // NR21: 50% duty, length
                   0x83,   // NR22: Volume 8, envelope down, step 3
                   0x40,   // NR23: Frequency low (lower than note 0)
                   0x87);  // NR24: Frequency high, restart sound
            break;

        case 2:
            // Length enabled (0xC7, not 0x87) — this is the LAST note of the
            // melody and the wave channel has no envelope, so left at 0x87 it
            // would ring forever (same bug fixed in the entrance fanfare's own
            // channel-3 note — see that one's comment for the full explanation).
            PlayFx(CHANNEL_3, BOSS_DEFEAT_MELODY_NOTE_DURATION,
                   0x80,   // NR30: Wave channel on
                   0xDE,   // NR31: Length ~130ms, self-stopping
                   0x20,   // NR32: Volume 50%
                   0x10,   // NR33: Frequency low (lowest, crumble finish)
                   0xC7);  // NR34: Frequency high, length enabled, restart sound
            break;
    }
}

/**
 * Starts the boss-defeat melody — call once from Boss.c's HandleDeath (both
 * the main boss splitting and a split copy's final death use this same cue).
 * Short enough (3 notes) to fire-and-forget without an Update* call tied
 * into it — the dying sprite is removed the same frame, so nothing would be
 * left to tick a per-frame Update anyway. The 3 PlayFx calls below just
 * queue on 3 different channels back-to-back on the same frame.
 */
void PlayBossDefeatMelody(void) BANKED {
    PlayBossDefeatMelodyNote(0);
    PlayBossDefeatMelodyNote(1);
    PlayBossDefeatMelodyNote(2);
}

/**
 * Plays the spread-shot cue (Boss.c's FireAtPlayer, the 2 extra diagonal
 * shots fired by BOSS_VARIANT_TRIPLE_SHOT only). Channel 3 — deliberately a
 * different channel than the aimed shot (channel 1) so the two attack types
 * are distinguishable by ear, not just by bullet pattern. Length-enabled
 * (0xC7, see the entrance-fanfare note above for why) so it can't repeat the
 * "rings forever" bug on this channel.
 */
void PlayBossSpreadShotSound(void) BANKED {
    PlayFx(CHANNEL_3, BOSS_SPREAD_SHOT_MUTE_FRAMES,
           0x80,   // NR30: Wave channel on
           0xE0,   // NR31: Length ~80ms — short, this can fire alongside the aimed shot
           0x20,   // NR32: Volume 50%
           0x40,   // NR33: Frequency low
           0xC7);  // NR34: Frequency high, length enabled, restart sound
}

/**
 * Player-takes-damage cue for the boss run / boss fight specifically — see
 * the header declaration for why this isn't just PlayPlayerHitSound reused.
 * Heavier/lower than that one and placed on channel 1 (shared with the arena
 * music's bassline below) on purpose: briefly ducking the beat on a real hit
 * reads as impact, not a glitch.
 */
void PlayBossArenaDamageSound(void) BANKED {
    PlayFx(CHANNEL_1, BOSS_DAMAGE_MUTE_FRAMES,
           0x15,   // NR10: Sweep down, time 1, shift 5 — a heavier, slower droop than the boss's own shot
           0xC2,   // NR11: 75% duty, length 2
           0xC4,   // NR12: Volume 12, envelope down, step 4 — hits hard, fades slower than a regular blip
           0x08,   // NR13: Frequency low (very low — a gut-punch, not a beep)
           0x87);  // NR14: Frequency high, restart sound
}

/*
 * ---- Arena background music (StateBossRun / StateBossFight) ----
 *
 * Not composed in hUGETracker: the .uge project format used by every other
 * track in this game (see the files under res/music, wired via
 * DECLARE_MUSIC/PlayMusic in the state files) is a complex binary format — instruments, wave tables,
 * pattern order — with no editor available in this environment, the same
 * kind of limitation as not being able to draw new pixel art in GBTD. This
 * is a from-scratch alternative built entirely out of raw PSG register
 * writes (PlayFx), same as every other sound in this file, just organized as
 * a tiny repeating step sequencer instead of a one-shot: a fixed-length
 * pattern of notes per channel, one step advanced on a timer, looping
 * forever. Deliberately NOT run through PlayMusic/hUGEDriver — that system
 * and this one would fight over the exact same 4 hardware channels, so
 * StateBossRun/StateBossFight must not call PlayMusic while this is active.
 *
 * Channel layout: 1 = bassline, 2 = lead/arpeggio, 4 = drums (kick/hat).
 * Channel 3 is left free for one-shot stingers (spread-shot cue, defeat
 * melody, entrance fanfare) so they don't fight the music for a channel.
 * Channels 1/2/4 WILL still get momentarily stolen by the boss's own action
 * sounds (move/shoot/sword/damage) — accepted rather than solved, same as
 * the rest of this file: on real GB hardware, action stingers ducking the
 * beat for a frame is normal chiptune-boss-fight texture, not a bug.
 *
 * Note frequencies below are the standard GB PSG formula, F = 2048 -
 * 131072/freq_hz, precomputed for a small set of low-register notes (this
 * project's existing SFX never used anything below ~C5 — no bass weight at
 * all, part of why everything read as "thin beeps"; deliberately fixed here).
 */
#define NOTE_REST 0 // never a real note in these tables — 0Hz is never used on purpose

#define NOTE_C2  44
#define NOTE_CS2 157
#define NOTE_D2  263
#define NOTE_EB2 363
#define NOTE_E2  458
#define NOTE_F2  547
#define NOTE_FS2 631
#define NOTE_G2  711
#define NOTE_AB2 786
#define NOTE_A2  856
#define NOTE_B2  987
#define NOTE_C4  1547
#define NOTE_D4  1602
#define NOTE_E4  1650
#define NOTE_FS4 1694
#define NOTE_G4  1714
#define NOTE_A4  1750
#define NOTE_B4  1783
#define NOTE_C5  1798
#define NOTE_D5  1825
#define NOTE_E5  1849

#define NR_LO(f) ((f) & 0xFF)
#define NR_HI(f) (0x80 | (((f) >> 8) & 0x07)) // trigger + freq high bits, length disabled (retriggered every step anyway)

#define MUSIC_PERC_NONE  0
#define MUSIC_PERC_KICK  1
#define MUSIC_PERC_HAT   2
#define MUSIC_PERC_SNARE 3

// Boss-run pattern (polish pass 2 2026-09-14: was 2 bars/16 steps looping
// every ~1.9s — extended to 4 bars/32 steps (~3.7s) with 2 more chord changes
// (Em -> D -> C -> G, a full descending-then-turnaround progression) instead
// of immediately looping back after just 2 bars, so there's real harmonic
// movement before it repeats.
#define BOSSRUN_MUSIC_PATTERN_LEN 32
static const UINT16 bossrun_bass[BOSSRUN_MUSIC_PATTERN_LEN] = {
    // Bar 1 (Em)
    NOTE_E2, NOTE_G2, NOTE_B2, NOTE_G2, NOTE_E2, NOTE_G2, NOTE_A2, NOTE_G2,
    // Bar 2 (D)
    NOTE_D2, NOTE_FS2, NOTE_A2, NOTE_FS2, NOTE_D2, NOTE_FS2, NOTE_G2, NOTE_FS2,
    // Bar 3 (C) — new
    NOTE_C2, NOTE_E2, NOTE_G2, NOTE_E2, NOTE_C2, NOTE_E2, NOTE_G2, NOTE_E2,
    // Bar 4 (G, turnaround back to Em) — new
    NOTE_G2, NOTE_B2, NOTE_D2, NOTE_B2, NOTE_G2, NOTE_B2, NOTE_A2, NOTE_B2
};
static const UINT16 bossrun_lead[BOSSRUN_MUSIC_PATTERN_LEN] = {
    NOTE_REST, NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_G4,
    NOTE_REST, NOTE_D4, NOTE_REST, NOTE_FS4, NOTE_REST, NOTE_A4, NOTE_REST, NOTE_FS4,
    NOTE_REST, NOTE_C4, NOTE_REST, NOTE_E4, NOTE_REST, NOTE_G4, NOTE_REST, NOTE_E4,
    NOTE_REST, NOTE_G4, NOTE_REST, NOTE_B4, NOTE_REST, NOTE_D5, NOTE_REST, NOTE_B4
};
static const UINT8 bossrun_perc[BOSSRUN_MUSIC_PATTERN_LEN] = {
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK,  MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_SNARE // last step: double snare fill, cues the loop-back
};
#define BOSSRUN_MUSIC_STEP_FRAMES 7 // slightly faster than before — reads as more energetic

// Boss-fight pattern (polish pass 2 2026-09-14: was 1 bar/8 steps looping
// every ~0.8s — extended to 2 bars/16 steps (~1.6s). Bar 1 is the original
// creeping Cm bass; bar 2 is a full chromatic climb (C->G) under a rising
// lead line, a classic "tension building" motif, before it drops back to
// bar 1's creep. Drums also gained the same kick/hat/snare backbeat as the
// boss-run track (was plain kick/hat alternating).
#define BOSSFIGHT_MUSIC_PATTERN_LEN 16
static const UINT16 bossfight_bass[BOSSFIGHT_MUSIC_PATTERN_LEN] = {
    // Bar 1: creeping Cm
    NOTE_C2, NOTE_C2, NOTE_CS2, NOTE_C2, NOTE_G2, NOTE_G2, NOTE_AB2, NOTE_G2,
    // Bar 2: chromatic climb C2->G2 — new, rising tension before the drop back to bar 1
    NOTE_C2, NOTE_CS2, NOTE_D2, NOTE_EB2, NOTE_E2, NOTE_F2, NOTE_FS2, NOTE_G2
};
static const UINT16 bossfight_lead[BOSSFIGHT_MUSIC_PATTERN_LEN] = {
    NOTE_C5, NOTE_REST, NOTE_C5, NOTE_REST, NOTE_D5, NOTE_REST, NOTE_D5, NOTE_REST,
    NOTE_C5, NOTE_REST, NOTE_D5, NOTE_REST, NOTE_E5, NOTE_REST, NOTE_E5, NOTE_REST
};
static const UINT8 bossfight_perc[BOSSFIGHT_MUSIC_PATTERN_LEN] = {
    MUSIC_PERC_KICK, MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK, MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK, MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_HAT,
    MUSIC_PERC_KICK, MUSIC_PERC_HAT, MUSIC_PERC_SNARE, MUSIC_PERC_SNARE
};
#define BOSSFIGHT_MUSIC_STEP_FRAMES 6 // faster than the run — more urgent

static const UINT16* music_bass_pattern;
static const UINT16* music_lead_pattern;
static const UINT8*  music_perc_pattern;
static UINT8 music_pattern_len;
static UINT8 music_step_frames;
static UINT8 music_step_timer;
static UINT8 music_step_index;
static UINT8 music_playing;

static void PlayMusicBassNote(UINT16 f) {
    if (f == NOTE_REST) return;
    PlayFx(CHANNEL_1, ARENA_MUSIC_MUTE_FRAMES,
           0x00,             // NR10: No sweep
           0xC0,             // NR11: 75% duty (fuller/rounder — this is the bass), length 0
           0x84,             // NR12: Volume 8, envelope down, step 4
           NR_LO(f), NR_HI(f));
}

static void PlayMusicLeadNote(UINT16 f) {
    if (f == NOTE_REST) return;
    PlayFx(CHANNEL_2, ARENA_MUSIC_MUTE_FRAMES,
           0x80,             // NR21: 50% duty, length 0
           0x73,             // NR22: Volume 7, envelope down, step 3
           NR_LO(f), NR_HI(f));
}

static void PlayMusicPercStep(UINT8 kind) {
    if (kind == MUSIC_PERC_KICK) {
        PlayFx(CHANNEL_4, ARENA_MUSIC_MUTE_FRAMES,
               0x00,   // NR41: Length
               0xA6,   // NR42: Volume 10, envelope down, step 6 — punchy but not too long
               0x07,   // NR43: Shift 0, wide/full noise, high divisor — deep low thud
               0x80);  // NR44: Restart sound
    } else if (kind == MUSIC_PERC_HAT) {
        PlayFx(CHANNEL_4, ARENA_MUSIC_MUTE_FRAMES,
               0x00,   // NR41: Length
               0x62,   // NR42: Volume 6, envelope down, step 2 — short, quiet
               0x29,   // NR43: Higher shift, narrow noise — crisp metallic tick
               0x80);  // NR44: Restart sound
    } else if (kind == MUSIC_PERC_SNARE) {
        // Mid-pitched, broader-band noise than the hat and punchier than the
        // kick — sits on the backbeat (boss-run pattern) so the groove reads
        // as a real drum beat instead of just kick/hat alternating.
        PlayFx(CHANNEL_4, ARENA_MUSIC_MUTE_FRAMES,
               0x00,   // NR41: Length
               0x83,   // NR42: Volume 8, envelope down, step 3
               0x32,   // NR43: Mid shift, wide noise, low divisor — broadband snap
               0x80);  // NR44: Restart sound
    }
}

static void StartArenaMusic(const UINT16* bass, const UINT16* lead, const UINT8* perc, UINT8 pattern_len, UINT8 step_frames) {
    music_bass_pattern = bass;
    music_lead_pattern = lead;
    music_perc_pattern = perc;
    music_pattern_len = pattern_len; // boss-run (16 steps) and boss-fight (8) differ — not a shared fixed length
    music_step_frames = step_frames;
    music_step_index = 0;
    music_playing = 1;

    PlayMusicBassNote(bass[0]);
    PlayMusicLeadNote(lead[0]);
    PlayMusicPercStep(perc[0]);

    music_step_timer = step_frames;
}

static void UpdateArenaMusic(void) {
    if (!music_playing) return;

    if (--music_step_timer == 0) {
        music_step_timer = music_step_frames;
        music_step_index++;
        if (music_step_index >= music_pattern_len) music_step_index = 0;

        PlayMusicBassNote(music_bass_pattern[music_step_index]);
        PlayMusicLeadNote(music_lead_pattern[music_step_index]);
        PlayMusicPercStep(music_perc_pattern[music_step_index]);
    }
}

/**
 * Starts the boss-run level music — call once from StateBossRun's START()
 * (alongside PlayBossEntranceMelody; the two briefly overlap on channels
 * 1-3 for about half a second, which reads as a sting leading into the
 * groove rather than a conflict).
 */
void PlayBossRunMusicStart(void) BANKED {
    StartArenaMusic(bossrun_bass, bossrun_lead, bossrun_perc, BOSSRUN_MUSIC_PATTERN_LEN, BOSSRUN_MUSIC_STEP_FRAMES);
}

/** Advances the boss-run music — call every frame from StateBossRun's UPDATE(). */
void PlayBossRunMusicUpdate(void) BANKED {
    UpdateArenaMusic();
}

/** Starts the final-boss arena music — call once from StateBossFight's START(). */
void PlayBossFightMusicStart(void) BANKED {
    StartArenaMusic(bossfight_bass, bossfight_lead, bossfight_perc, BOSSFIGHT_MUSIC_PATTERN_LEN, BOSSFIGHT_MUSIC_STEP_FRAMES);
}

/** Advances the final-boss arena music — call every frame from StateBossFight's UPDATE(). */
void PlayBossFightMusicUpdate(void) BANKED {
    UpdateArenaMusic();
}

/**
 * Loads a known waveform into wave RAM (0xFF30-0xFF3F).
 * Why: every channel-3 sound effect below just turns the wave channel on and
 * plays whatever wave RAM holds. On a real DMG that memory is random at power
 * up (emulators usually start it clean), so those effects could sound harsh
 * on hardware. hUGE songs that use channel 3 overwrite it themselves when they
 * start, so call this right BEFORE PlayMusic.
 * Wave RAM may only be written safely with the channel-3 DAC off, so NR30 is
 * switched off for the copy and restored afterwards.
 */
void InitWaveRam(void) BANKED {
    // Triangle wave: 32 samples of 4 bits, two samples per byte.
    static const UINT8 triangle_wave[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    UINT8 i;
    UINT8 saved_nr30 = NR30_REG;

    // Interrupts off during the copy: the music timer interrupt also drives
    // channel 3 and must not interleave with these register writes.
    CRITICAL {
        NR30_REG = 0x00;
        for (i = 0; i != 16; ++i) {
            AUD3WAVE[i] = triangle_wave[i];
        }
        NR30_REG = saved_nr30;
    }
}
