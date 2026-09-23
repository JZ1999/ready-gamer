# Session notes (local — read first each session)

Last updated: 2026-09-23. Branch: `feature/map5-spawn-locks`.

## >>> RESUME HERE (2026-09-23) — full record <<<

**Where we stand:** everything from 09-21/09-22 (below) plus a real crash fix are **committed and pushed** to `origin/feature/map5-spawn-locks` (6 commits, up through `bae231c`). **Not merged to main, no PR opened, not flashed.**

**Real bug found and fixed:** entering the map5→autoscroll portal reliably threw a spurious in-room "GAME OVER! Press any key" instead of transitioning to `StateBossRun` — 100% repro, both BGB and Emulicious, independent of wave/coins, independent of how the player reached map5 (confirmed NOT reproducible booting straight into `StateBossRun` from the menu, which is what pointed at the room→bossrun transition specifically). Root cause: `StateGame.c`'s `next_room >= room_count` compared against `room_count`, a `const UINT8` that physically lives in `Rooms.c`'s own ROM bank — reading it directly from StateGame's own (different) bank with no bank switch is undefined; it can read whatever byte happens to be at that address in the wrong bank. It let `next_room` slip past the check into `LoadRoomFromTable(MAX_ROOMS)`, which sets `current_room = MAX_ROOMS` (invalid, only 0-4 are real rooms) with no bounds check of its own. Debugged by adding a temporary on-screen print (room/level/lives/pool-count) inside `CheckForPlayerDeath()` — it showed `room=5`, the smoking gun. **Fix:** compare against `MAX_ROOMS` (compile-time `#define`) instead of the runtime `room_count` extern. Full technical writeup saved to memory: `reference_gb_bank_switch_hazard.md` (new "reading DATA across banks" section).

**Content changes, same session:** coins now have a per-pickup value (`CoinPickupPlacement` + `custom_data[CD_COIN_VALUE]`, replacing the old single global `COIN_PICKUP_VALUE=5` constant) — added a 3-coin pickup on map5 at tile (28,15). Opened an intentional wall gap in map5 at tiles (42,9)-(43,9). Re-positioned 2 of 3 spawns + the coin on map3 (room2), and tile-aligned all 3 spawns on map2 (room1) — several were sitting at off-grid pixel coordinates from earlier hand-authoring.

**Tool paths found (save future searching):** Emulicious (`ZGB_extracted/ZGB/env/emulicious/Emulicious.exe`) and GBTD/GBMB (`ZGB_extracted/ZGB/env/tools/gbtd22/GBTD.EXE`, `.../gbmb18/GBMB.EXE`) all ship bundled inside the ZGB engine archive, not installed system-wide.

**Known cosmetic bug, not fixed:** stray "A" glyphs from `StateGameOver.c`/`StateWin.c`'s `PRINT_BKG` "PRESS A" text can persist in background VRAM the room game doesn't fully overwrite, becoming visible once the camera later scrolls to that spot. Doesn't affect collision/gameplay.

**Debug scaffolding confirmed clean before the push:** `DEBUG_START_ROOM=0`, `DEBUG_START_LEVEL=1`, `DEBUG_START_COINS=0`, `PROJECTILE_DAMAGE_NORMAL=1`, `PROJECTILE_DAMAGE_ELECTRIC=2`, `StateMenu.c` boots to `StateGame` normally.

**Open questions:** PR to main — when? 5s between waves still not explicitly confirmed as feeling right. Cartridge/flasher for physical hardware still undecided; nothing flashed yet.

**`INIT.md` created this session** — new local (gitignored) per-project entry point per updated global `CLAUDE.md` rules. Read that first from now on, before this file.

---

## >>> RESUME HERE (2026-09-21, end of day) — full record <<<

**Where we stand:** hardware-ready Release ROM for a physical **MBC5** cartridge exists and passed all headless + emulator checks. **Nothing is committed** (last commit still `2169a7a`). User was playing map 4 in Emulicious ("va todo bien") and stopped for the day. Not flashed yet.

**Final ROM:** `bin/READY_GAMER.gb`, 131072 bytes, SHA-256 `018e40ba8817eb6d92265ba48945bafa9481c63a2d44746dc1a7cdc98d1547ed`. Header: type `0x19` (MBC5), ROM size code 2 (128 KB), no RAM/battery, title `READY GAMER`, DMG-only (CGB flag 0), header checksum and global checksum verified. Permanent copies + SHA256SUMS + test scripts: `E:\Users\Alejandro\Opal\Game-Boy-releases\2026-09-21\` (outside the repo; the `bin/` folder gets wiped by builds).

**Everything changed today (all uncommitted):**
1. `src/Makefile`: `BINFLAGS += -yt 0x19 -yn "READY GAMER"` (MBC5 + title, after the engine's own `-yt 1`; last one wins) and `CFLAGS += -UNDEBUG` (see 6).
2. `src/states/StateGame.c`: `next_round_timer` UINT8 -> UINT16. `NEXT_ROUND_TIMER 300` used to overflow to 44 (~0.7 s between waves); now 5 s. Balance knob = the `NEXT_ROUND_TIMER` define (line ~19). User has NOT yet said whether 5 s feels right.
3. `src/systems/SoundEffects.c` + `include/SoundEffects.h`: `InitWaveRam() BANKED` loads a triangle wave into wave RAM (0xFF30-3F) inside `CRITICAL`; called from `StateMenu.c START()` before `PlayMusic`. Reason: channel-3 SFX play whatever wave RAM holds, random on a real DMG.
4. `include/SoftReset.h` (new) + `CHECK_SOFT_RESET()` at the top of `UPDATE()` in `StateGame.c`, `StateBossRun.c`, `StateBossFight.c`: A+B+START+SELECT held -> GBDK `reset()`.
5. `src/systems/Rooms.c`: room 3 (map4) door P1 moved one tile right, tile (2,10) -> (3,10) (user request). It now plugs the 1-tile gap in wall column x=3. Side effect: left corridor (x=1-2) no longer needs P1; the bottom-left dead-end pocket is reachable without paying. Comment `Doors: P1=(3,10)` updated.
6. **CRITICAL bug fixed:** Release compiled the HUD out. ZGB `Print.h` turns `INIT_CONSOLE`/`DPrintf`/`DPRINT_POS` into no-ops under `NDEBUG` (Release defines it). The game draws its HUD with them, and `INIT_CONSOLE` also calls `SetWindowY`, which makes the LYC interrupt (`LCD_isr`) show sprites. Release therefore showed an empty room: no HUD, no player, no doors. The 09-15 "room 0 has no sprites" report and the 09-18 hardware Release were this same bug. Only Debug builds had ever been played. `CLAUDE.md` got a warning line about it.
7. Earlier uncommitted work still in the tree from 09-18: sprite pool guard (`include/SpriteBudget.h`, `SafeSpriteAdd*` in BomberVirus, BossFightPlayer, BossRunPlayer, ElectricProjectile, SpritePlayer, SpriteScrew, StateBossRun, StateGame, Rooms.c `SyncSpawnMarkers`). `StateGame.c` DEBUG values are at release values (room 0, level 1, coins 0, `STARTING_LIVES`); `SpriteData.h` damage 1/2.

**Verification done (no crash found):**
- PyBoy headless (`pip install pyboy pillow numpy`): boot -> menu -> Start -> room 0 with HUD `LEVEL 1 / COINS:0 LIVES:3` and 12 sprites; soft reset returns to menu; 8/8 boots with RAM/VRAM/OAM/HRAM filled with pseudo-random bytes reach menu and a full room 0; 25-40 s random-input play in rooms 0-4 (level 20 in rooms 1-4) with no crash/freeze, peak 15 of 20 sprites; boss run and boss fight 60 s each; Win and GameOver draw and return to menu; room 3 door P1 at the new spot still blocks the horizontal corridor.
- Emulators: BGB and Emulicious show the same game (user confirmed). Emulicious was open with the final ROM when the user left (they were playing map 4).
- Static: crt0 zeroes `_DATA` and shadow OAM; stack at 0xDEFF with ~4.7 KB free; only ROM-range write is `0x2000`; `display_off` waits for vblank; `wait_vbl_done` returns at once with LCD off; boss fight worst case ~13 of 20 sprites so unguarded `Boss.c` stays safe.
- Bank free bytes (final): bank 0 = 248, bank 1 = 1851, bank 2 = 20, bank 3 = 23, banks 4-6 mostly free, bank 7 empty. Maps pinned to banks 2,2,3,3,4,5,6. Any rebuild that changes code can shift things: re-verify header, checksums and bank map, then freeze.

**NOT verified:** sound on real hardware, real DMG LCD ghosting/timing, room-3 frame budget (~18 sprites), the physical cartridge itself, a full manual playthrough of all 5 rooms + boss run + boss fight with Emulicious "Read as random" and the hardware exceptions on (that option is menu-only, not settable via Emulicious.ini).

**Known minor risk (left as is, cosmetic):** an interrupt (music timer, LYC, VBL) landing between the STAT check and the write in `SetTile`/`set_bkg_tile_xy` can push a VRAM write into mode 3, where hardware drops it: rare single wrong background tile until redrawn. Collision uses the room table, so gameplay is unaffected. Fix would need a `di/ei` patch in the ZGB engine (`ZGB_extracted`, gitignored; `ZGB.zip` is tracked).

**Explicit decisions (do not re-litigate):** option C (portal on demand) and D (max 1 Bomber per wave) declined; do NOT touch `src/sprites/Boss.c`; balance numbers are the user's; nothing gets committed/pushed without an explicit OK.

**Open questions for the user tomorrow:** (a) commit now? one commit or several? (b) 5 s between waves OK? (c) which cartridge / flasher, and does it honour the MBC5 header? (d) any exception from Emulicious? (e) move doors P2/P3 too?

**Suggested next steps:** 1) user finishes the manual playthrough on Emulicious (random memory + exceptions on); 2) commit (ask first); 3) confirm the frozen ROM hash; 4) flash `READY_GAMER_FINAL_018e40ba.gb` (never a Debug ROM), mapper MBC5, no RAM; 5) test on the device: menu, music, room 0, a door, portal, boss run, boss fight, reset combo; 6) report anything that differs from the emulator.

**Gotchas learned today:** an emulator holding `bin/` makes the automatic `make clean` fail with "Permission denied" (a Makefile change triggers that clean). Workaround: delete `Release/`, `mkdir Release`, `touch Release/Makefile.uptodate`, then `make build_gb BUILD_TYPE=Release` from `src/` with `ZGB_PATH` set. Building Release wipes the Debug ROM in the shared `bin/`. Python on Windows needs `C:\...` paths. GUI-clicking the user's desktop is unsafe (screen capture shows their other apps); use PyBoy for automated checks. The first "broken Emulicious" captures were most likely the user's own window showing the Release no-HUD bug, not random RAM.

---


## Session 2026-09-15: reverted debug boot to normal game, mobs stuck on walls in map4/room3 maze

**Reverted `StateMenu.c` DEBUG wiring**: START/A was still going to `StateBossRun` (leftover from the 2026-09-14 boss-run/fight testing session) instead of the normal campaign. Changed back to `SetState(StateGame)` — normal boot flow restored (`StateMenu` → `StateGame`, `DEBUG_START_ROOM=3` still in place so it boots straight into map4/room3 for continued testing there, per existing convention — not reverted, kept on purpose).

**New bug found via screenshot (map4/room3 maze)**: `BasicVirus` enemies get visibly stuck against walls, not moving, near corners/dead-ends in the maze corridors. Root cause identified: `EnemyMoveWithWallAvoidance` (`ZGBMain.c`) only tries a **Y-axis** avoidance nudge when the primary chase movement is blocked — never tries X. If the Y nudge is *also* blocked (a corner or narrow dead-end, common in this maze layout), the enemy has no fallback and just sits there every frame.

**Tried option 1 (bidirectional avoidance) — did NOT fix it, reverted.** Generalized `EnemyMoveWithWallAvoidance` to fall back to an X-axis nudge (toward-target-then-opposite, same pattern as the existing Y fallback) when both Y attempts also failed. Rebuilt, playtested by the user in the same map4/room3 maze spot — **enemies still got stuck**. Reverted cleanly (`ZGBMain.c` back to exact pre-change state, diff-confirmed). **User asked to pause and think before trying anything else** — do not assume any of the other discussed options (wall-hug state machine, jitter, stuck-timer teleport fallback, full pathfinding) yet; wait for direction next session.

**Worth reconsidering next time this comes up**: since adding a *second* axis of avoidance didn't help, the stuck spots in the screenshot may not be a "both axes blocked" case at all — could be something else entirely (e.g. `WALL_COLLISION_MARGIN`/hitbox size vs. this maze's exact corridor geometry, the chase-toward-`scroll_target` logic picking a dx/dy of 0 on the blocked axis so the avoidance branch never even triggers, or the enemy's move timer/state getting stuck some other way not in `EnemyMoveWithWallAvoidance` at all). Don't re-try more avoidance-branch variants blind — worth instrumenting (temporary on-screen debug print of the enemy's attempted dx/dy and the `result` value at the stuck moment, same technique used for the earlier spawn-lock bug hunt) before guessing again.

**Committed** (`8adf611`, on top of `9cbe8d6`): all of 2026-09-14's boss run/fight music+sound work, the level-1 autoscroll enemies, and the `StateMenu.c` debug-boot revert. Before committing, `DEBUG_START_ROOM`/`DEBUG_START_COINS`/`player_lives` (`StateGame.c`) were temporarily set back to normal (0/0/`STARTING_LIVES`), rebuilt clean to confirm, committed, then **immediately restored to the debug testing values** (`DEBUG_START_ROOM=3`, `DEBUG_START_COINS=50`, `player_lives=DEBUG_STARTING_LIVES`) uncommitted again — same "flag before shipping, keep for testing" convention as always. `ZGBMain.c` is back to its pre-session state (the avoidance experiment left no trace, not part of this commit).

**Working tree right now**: only `src/states/StateGame.c` modified (the 3 debug values above), nothing else uncommitted.

## Session 2026-09-15 (continued): mobs-stuck-on-walls — root cause found, real fix attempted (option 1 was NOT it)

**Real root cause identified** (`BasicVirus.c`): the chase AI moves on a single greedy axis per tick (X takes priority over Y whenever misaligned) and, when blocked, only ever nudges *toward* the player on the perpendicular axis (plus, after the reverted option-1 experiment, the reverse of the primary axis too) — a **1-step lookahead**. A real maze can have pockets more than one tile deep where escaping requires moving *away* from the player for several consecutive tiles before the path bends back — no amount of single-step nudging escapes that, which is why option 1 (bidirectional single-step avoidance) didn't help. This is the same underlying limitation that motivated the abandoned flow-field pathfinder attempt on 2026-09-09.

**Fix implemented (decided without asking, per explicit request this turn)**: a stuck/wander escape hatch in `BasicVirus.c`, not a real pathfinder — cheaper, self-contained, easy to revert if it doesn't hold up:
- New packed byte `CD_WANDER_STATE` (slot 6: `(steps_remaining << 2) | direction`) + `CD_STUCK_COUNT` (slot 7, plain counter) — both free slots for this enemy type (health/frame/blink/move use 0-3; ChargeVirus is the only virus type that already uses 6/7, untouched here since this change is BasicVirus-only).
- Normal ticks: unchanged greedy chase. Each blocked attempt increments `CD_STUCK_COUNT`; after `STUCK_THRESHOLD` (5) consecutive blocked ticks (~0.8s at `ENEMY_SPEED=10`), abandon greedy chasing and commit to `WANDER_STEPS` (6) ticks in one `rand()`-picked cardinal direction, ignoring the player's position entirely for that stretch.
- While wandering: keep moving in the committed direction; if that also hits a wall, immediately re-roll a new random direction (don't wait through the full threshold again); once the committed steps run out (or a step is blocked and re-rolled), resume normal chase.
- Applies uniformly whether chasing the room-game player or `boss_run_player` (shared `CustomTranslateSprite`/`UPDATE()` path) — not expected to cause problems in the sparser autoscroll corridor, just rarely triggered there.
- Only touched `BasicVirus.c` — the other 4 enemy types (SpeedVirus, TankVirus, BomberVirus, ChargeVirus) have their own copies of this same greedy-chase pattern and were **not** changed, since the reported bug was specifically BasicVirus in the map4/room3 maze. Revisit if the same complaint comes up for another enemy type.

Rebuilt clean (only the pre-existing baseline warnings, one SDCC "optimizer" notice on unchanged code that just shifted line number — same pattern documented in this file before). **Not yet confirmed by the user** — launched in BGB, waiting on a playtest verdict in the map4/room3 maze before considering this done. If it *still* doesn't hold up, the next honest option is revisiting the reverted flow-field pathfinder (2026-09-09 section above) rather than another local-heuristic patch — don't keep iterating on single-step nudge variants, that space is exhausted.

Nothing committed this round.

## Session 2026-09-15 (continued): stuck fix still not enough — wander distance was way too short

User sent a screenshot: 2 `BasicVirus` instances exactly overlapping (same position, no enemy-enemy collision exists in this engine — confirmed by asking, not guessed), both hugging the same wall corner. Root cause: `WANDER_STEPS` was `6` — **6 pixels**, less than one tile — nowhere near enough to clear a real dead-end in a maze with 16px-wide corridors. The enemy would nudge a few pixels, then immediately resume greedy chase straight back into the same trap, which is why the escape looked like it wasn't doing anything at all. **Fixed**: `WANDER_STEPS` raised to `18` (a bit over 2 tiles), `STUCK_THRESHOLD` lowered `5→3` (reacts faster, ~0.5s instead of ~0.8s at `ENEMY_SPEED=10`). BasicVirus-only, same as the original fix. Rebuilt clean, launched in BGB. **Not yet confirmed** — next playtest should specifically watch whether the same corner spot clears now, and whether the two overlapping instances actually separate (they should, since their `rand()` calls land on different values, but nothing repels them from each other if they do independently end up on the same only-viable path back out).

## Session 2026-09-15 (continued): diagonal + wander kept as final tuning, all debug scaffolding reverted for a ship-ready build

Per explicit request ("déjalo así... nada extra, listo para lanzar"): the diagonal-chase (every 3rd tick) and BasicVirus wander/stuck-escape fixes above are being kept as-is, no further tuning requested. All `StateGame.c` `DEBUG_*` testing scaffolding reverted to real launch values:
- `DEBUG_START_ROOM` → `0` (boots into room 0, not map4/room3).
- `DEBUG_START_COINS` → `0` (no starting coin buff).
- `DEBUG_STARTING_LIVES` → `STARTING_LIVES` (no more 100-lives boss-testing override; real 3 lives applies everywhere, including carried into boss run/fight).
- `DEBUG_START_ELECTRIC` was already `0`. `StateMenu.c` was already normal (`SetState(StateGame)` on START/A, reverted earlier this session).
- Confirmed no other `DEBUG_*` scaffolding anywhere else in `src/`/`include/` (grepped the whole tree).

Rebuilt **both** variants clean: `BUILD_TYPE=Debug` (baseline warnings only) and, since this is meant to be a real launch-ready build, `BUILD_TYPE=Release` (`bin/READY_GAMER.gb`) — first-ever Release build this session, full rebuild from scratch, same benign "EVELYN the modified DOG" optimizer notices scattered across several pre-existing files (ChargeVirus, SpritePlayer, BossRunPlayer, BossFightPlayer, SpeedVirus, TankVirus, BomberVirus — all on unrelated pre-existing code, not new). Both launched in BGB to confirm normal boot (no debug shortcuts, real room 0 start).

**Nothing committed this round** — working tree has `StateGame.c` (debug revert) + all of the diagonal/wander enemy AI work (`BasicVirus.c`, `SpeedVirus.c`, `TankVirus.c`, `BomberVirus.c`) uncommitted, on top of the `8adf611` commit from earlier today. Ask before committing/pushing.

## Session 2026-09-15 (continued): room 0 (level 1) reported with no visible sprites — clean rebuild done, unconfirmed; +10 damage debug added for faster room-by-room testing

**Real regression report**: user booted into room 0 (first time this session anyone actually tested room 0 fresh — every prior test this whole session used `DEBUG_START_ROOM=3`) and saw the map background render fine, but **no player, no doors, no portal, no spawn markers — zero sprites visible**, while background text (`Printf`/`DPrintf`, a completely separate BG-tile rendering path from sprites) worked correctly (confirmed by a second screenshot showing the door's "Need 10 Ready Coins!" message rendering fine while still no sprites visible). Since Door/Portal/Player code was untouched this session, this isn't from the diagonal/wander enemy AI work — root cause not confirmed, but the leading theory is a stale ROM-bank pinning artifact for room 0's map (`map`/bank via `BindScrollMap`'s `else` branch) from incremental builds, since nobody has done a truly clean build+fresh-room-0-boot together in a long time (every session for a long while has tested room3/room4 via `DEBUG_START_ROOM`).

**Action taken**: full `make clean` + rebuild for both `BUILD_TYPE=Debug` and `BUILD_TYPE=Release`, relaunched in BGB. **Not yet confirmed fixed** — told the user plainly this is a diagnosis, not a confirmed root cause; if it recurs on the clean-rebuilt ROM, it's a real bug that needs proper hunting (not a stale-build artifact), and none of rooms 0/1/2/4 have been re-verified this session (only room3/map4 got real playtesting after today's changes).

**+10 damage debug added per explicit request** (`include/SpriteData.h`): `PROJECTILE_DAMAGE_NORMAL` 1→11, `PROJECTILE_DAMAGE_ELECTRIC` 2→12, tagged `// DEBUG` — purpose is to kill enemies faster while doing the room-by-room verification pass (real 3-lives balance is still in effect, only damage is boosted). This project has used an equivalent "x10 damage" testing cheat before (see 2026-09-05 addendum) — same idea, different multiplier this time, must be reverted to `1`/`2` before shipping. Required `make clean` (header change) — done, Debug rebuilt clean, relaunched.

**Still open**: verify all 5 rooms boot with visible sprites, plus boss run/fight once more without the old 100-lives buff, before calling this ship-ready.

## Session 2026-09-15 (continued): real build-process bug found — building Release after Debug silently deletes the other variant's ROM from `bin/`

While reverting the +10 damage debug value, launched BGB against `bin/READY_GAMER_Debug.gb` and got a BGB window with **no ROM loaded** (generic `bgb` title, not `READY_GAMER_Debug.gb` like every other successful launch this session) — `ls bin/` confirmed the file genuinely didn't exist, only `READY_GAMER.gb` (Release) was present.

**Root cause, confirmed by reading `MakefileCommon`**: `BINDIR = ../bin` is **shared** across both build types (not per-`BUILD_TYPE` like `OBJDIR`, which correctly is `../$(BUILD_TYPE)`). Separately, `$(OBJDIR)/Makefile.uptodate: Makefile` auto-runs `make clean BUILD_TYPE=$(BUILD_TYPE)` whenever it thinks the Makefile changed for that variant's marker — and `clean:` does `@rm -rf $(BINDIR)`, wiping the **entire shared** `bin/` folder, not just that variant's own output. So building Debug then Release (or vice versa) back-to-back can silently delete the first variant's just-built ROM out from under it, with no error — the second build just reports success for its own file.

**This likely also explains the earlier "qué pasó en el nivel 1" confusion** — the "no sprites visible" screenshots may not have been a real game bug at all; if the user's BGB window ended up pointed at a since-deleted file (or was relaunched fresh against a missing path), BGB would show `bgb -` as a bare fallback title while potentially still displaying a stale last-rendered frame — matching the blank-title screenshots exactly. **Not certain** (didn't reproduce the invisible-sprites symptom specifically, only reproduced the missing-file/blank-title symptom) — worth keeping in mind before assuming room 0 has a real sprite-rendering bug; re-test room 0 with a freshly-verified-to-exist ROM before hunting further.

**Practical rule going forward this session**: after building both variants back-to-back, always re-verify with `ls bin/` (or equivalent) that the specific file about to be launched still exists — don't trust an earlier build's confirmation once a second `make BUILD_TYPE=...` call has run since. Rebuilt Debug once more just now, confirmed both `READY_GAMER.gb` and `READY_GAMER_Debug.gb` coexist, launched and confirmed the title shows correctly.

## Session 2026-09-15 (continued): diagonal chase movement, every 3rd tick, on 4 of 5 enemy types

Per explicit request: enemies should sometimes move diagonally toward the player instead of always fully correcting X before ever touching Y (the old behavior always picks X first, only moving Y once X is aligned).

**Implemented in `BasicVirus.c`, `SpeedVirus.c`, `TankVirus.c`, `BomberVirus.c`** — all 4 share the same greedy single-axis chase snippet, so the same change applies to each:
- New per-sprite `CD_DIAG_TICK` byte (packed into whichever `custom_data` slot was free per type — 4 for BasicVirus/SpeedVirus/TankVirus, 6 for BomberVirus since it already uses 4/5 for its bomb-drop timer), cycling 0→1→2→0... once per **movement** tick (not per frame — still gated by each type's existing `move_timer`/`ENEMY_SPEED`).
- On 2 of every 3 movement ticks: unchanged behavior (X corrected first, Y only if already X-aligned).
- On the 3rd tick: X and Y are both computed independently, so if the player is off-axis in both, the enemy commits to both this tick — a real diagonal step.
- The diagonal step is executed as **two separate single-axis `CustomTranslateSprite` calls** (X then Y), not one call with both deltas — matches the existing convention (`SpritePlayer.c`'s own diagonal input handling works the same way) and means a wall on one axis doesn't cancel progress on the other (naturally still slides), rather than relying on `CheckEdgeMapCollision`'s combined-dx+dy path, which no other sprite in the codebase currently exercises.
- `BasicVirus.c` additionally had to fold this into the existing stuck/wander state machine: diagonal ticks only apply during normal chase (not mid-wander-escape), and a diagonal tick only counts as "blocked" for the stuck counter if **both** axis attempts failed (either one succeeding is real progress, not stuck).

**`ChargeVirus.c` deliberately NOT touched** — its `custom_data` is fully packed (all 8 slots already used: health/frame/blink/move + its own charge-state/timer/dash-dx/dash-dy), so there's no free byte for a `CD_DIAG_TICK` counter without overloading an existing field's meaning (considered and rejected — e.g. reusing `CD_CHARGE_TIMER`'s value doesn't actually cycle usefully at the movement cadence, the math works out to a constant residue instead of rotating). Its own brief pre-dash walk phase keeps the old single-axis chase; the charge-dash itself (the sprite's real identity) is unaffected either way. Revisit only if this specific enemy's walk-in phase becomes worth the trouble.

Rebuilt clean — only the same benign SDCC "optimizer" (`EVELYN the modified DOG`) notices as always, on unrelated pre-existing hit-blink code that just shifted line numbers. **Not yet confirmed by the user** — launched in BGB, waiting on a playtest verdict (does the diagonal step read as more direct/aggressive chasing, and does it interact OK with the BasicVirus stuck-escape fix above in the map4/room3 maze).

Nothing committed this round.

## RESUME HERE (2026-09-15, separate open question): unconfirmed — does the off-camera enemy cleanup actually work?

User asked "ahora que desaparezcan al desaparecer de la cámara" (make enemies disappear when they leave camera view) — but this exact behavior was **already added in the same session**, one turn earlier (see "level-1 enemies" section right below): `BasicVirus.c`'s `UPDATE()` already removes the sprite once `(INT16)THIS->x + 16 < scroll_x` (i.e. once it's scrolled off the left edge of the camera, while `boss_run_player` is active).

**Not established whether this is:**
(a) the user re-stating the requirement before ever testing the last build, or
(b) a real bug report — they tested and it's NOT working.

Asked the user to clarify with specifics (do enemies stay visibly stuck off-screen? pop out mid-screen?) — **no answer received yet, session paused here at the user's request** ("guarda todo en memoria... mañana seguimos"). **Do not assume this is fixed or broken — ask again / get the specifics first**, then either confirm it already works or actually debug it (check: is `boss_run_player` reliably non-NULL when expected; does `(INT16)THIS->x` cast behave correctly across the full 1920px map width — it should, GBM map is only 240×8=1920px, well within INT16 range; is the move-timer gate anywhere accidentally skipping this check — it shouldn't, the removal check sits before the `move_timer` gate in the function).

## Session 2026-09-14 (continued further): level-1 enemies (BasicVirus) added to the auto-scroll level

Reused `BasicVirus` (the room game's own level-1 enemy, not a new sprite) inside `StateBossRun` — spawned periodically (`ENEMY_SPAWN_INTERVAL=240` frames) at a random tile row just ahead of the camera's right edge, checked against `BossRunTileBlocked` first so it doesn't spawn embedded in a wall pillar (skips that spawn tick if blocked, tries again next interval rather than forcing placement).

**Real latent bug found and fixed along the way**: `SpriteScrew.c` (the player's own bullet, already fired by `BossRunPlayer.c`) only branched its movement collision for `boss_fight_player` (StateBossFight), never for `boss_run_player` (StateBossRun) — meaning the player could already shoot in the auto-scroll corridor before today, but every bullet was silently using `SafeTranslateSprite` (room-tile collision, stale/wrong data with no active room) instead of `BossRunTranslateSprite`. Same bug class as the boss-fight `SpriteScrew` fix from an earlier session — added the missing `else if (boss_run_player)` branch.

**`BasicVirus.c` changes** (all gated behind the existing `boss_run_player` NULL-guard pattern — every change below is a no-op, byte-identical behavior, in the normal room game where `boss_run_player` is always NULL):
- Chases `boss_run_player` instead of `scroll_target` — in `StateBossRun`, `scroll_target` is `CameraDriver` (an invisible self-driving sprite, not the player), so chasing it unmodified would have chased the camera instead.
- Movement collision routes through `BossRunTranslateSprite` instead of `EnemyMoveWithWallAvoidance` (the room-tile wall-avoidance code), matching the established "any sprite shared between the room game and a non-room state must branch its own collision" rule (`CLAUDE.md`/[[reference_ready_gamer_sprite_pool_limit]] territory).
- Deals its own contact damage via `BossRunTakeDamage` — `BossRunPlayer.c` has no generic per-frame "scan all sprites for an enemy touching me" loop the way `SpritePlayer.c` does for the room game, so (same pattern as `BossBullet.c`) the enemy itself checks `CheckCollision` against `boss_run_player` and calls `BossRunTakeDamage` directly.
- Self-removes once fully scrolled off the left edge (`THIS->x + 16 < scroll_x`) — otherwise these enemies would live forever off-screen once left behind, eating into the shared 20-sprite pool for no reason (see [[reference_ready_gamer_sprite_pool_limit]]).

Killing them already worked with zero extra code: `SpriteScrew.c`'s enemy-hit loop uses `IsEnemyType(spr->type)` (a generic type check, not state-aware), and `BasicVirus` is already in that list — coins/kill-counter bookkeeping (`ready_coins`/`enemies_killed`) happens same as in a normal room, harmless during the boss run.

Verified: full `make build_gb BUILD_TYPE=Debug` compiles clean — 3 pre-existing warnings in `BasicVirus.c` (overflow-in-constant-conversion, optimizer notice) just shifted line numbers from the new code inserted above them, not new warnings. **Not yet playtested** — next step is confirming enemies actually spawn, chase, deal/take damage, and get cleared correctly in BGB.

## Session 2026-09-14 (continued): custom music sequencer for boss run/fight, 2 real bugs found via playtest, debug-entry correction

**Correction from the user, worth remembering going forward**: "start the game at X" means **menu START → X**, not skipping the menu at ROM boot. First pass wrongly changed `next_state` in `ZGBMain.c` (skips the menu entirely); reverted to `next_state = StateMenu`, and `StateMenu.c`'s START/A handler now has `SetState(StateBossRun)` tagged `// DEBUG` instead (revert to `SetState(StateGame)` for the normal flow). Saved as [[feedback-ready-gamer-debug-entry-point]] — this is now the standing convention for any future "boot into X for testing" request in this project.

**Real bug found via playtest: wave-channel (channel 3) notes with no length limit ring forever.** The boss-entrance-fanfare's 3rd note and the boss-defeat-melody's 3rd (last) note were both on channel 3 with `NR34=0x87` (length counter disabled) — channel 3 has no envelope, so with nothing to silence it and no length limit, that note just kept playing indefinitely instead of fading, which is what the user heard as a "shrill continuous beep" after the fanfare. **Fixed**: both now use `NR34=0xC7` (length-enable bit set) + a real `NR31` length value (~130ms) so they self-stop. Also pulled back the fanfare's peak note (was 0xC0, right at the top of the pitch range this file uses, ~2048Hz — genuinely screechy) to a less extreme peak (0xA8). **This same latent bug still exists, untouched, in the pre-existing door-opening and enemy-hit melodies** (`PlayDoorMelodyNote`/`PlayEnemyHitMelodyNote`, both use a mid-sequence or final channel-3 note the same unsafe way) — not fixed since it wasn't reported as a problem there (probably masked by other gameplay sounds retriggering channel 3 again soon after in normal play) and out of scope for what was asked, but worth knowing if a similar "why is there a background beep" report ever comes up outside the boss content.

**Built a custom PSG music sequencer** (`SoundEffects.c`) for continuous background music in `StateBossRun` and `StateBossFight` — the user asked for "real" level music, but this project's actual music system (hUGETracker `.uge` project files under `res/music/`, played via `DECLARE_MUSIC`/`PlayMusic`) needs a real tracker GUI to author; there's no editor available here and no way to hand-write that binary format reliably (raised this explicitly, user chose "build what you can from scratch" over reusing an existing track or waiting for a hand-composed one). What got built instead: a tiny looping step sequencer using the exact same raw-`PlayFx` approach as every other sound in this file — a fixed 8-step pattern per channel (bass on channel 1, lead/arpeggio on channel 2, kick/hat drums on channel 4), one step advanced per timer tick, looping forever. Two pattern sets: `bossrun_*` (driving E-minor-ish riff, ~133ms/step) and `bossfight_*` (lower/darker chromatic bass, faster ~100ms/step, denser drums). Deliberately **not** run through `PlayMusic`/hUGEDriver — that system and this one would fight over the same 4 hardware channels, so neither `StateBossRun` nor `StateBossFight` call `PlayMusic` while their custom music is active. Channel 3 is left free on purpose for one-shot stingers (spread-shot cue, defeat melody, entrance fanfare) so they don't compete with the continuous music for a channel — channels 1/2/4 do still get momentarily stolen by the boss's own action sounds (move/shoot/sword/damage), which is accepted as normal chiptune-boss-fight texture rather than something to solve.

**Removed** `PlayBossAmbiencePulse` (the periodic noise-channel pulse from earlier this session) — now redundant/actively conflicting with real continuous music covering the same "atmosphere" role in `StateBossFight`.

**3 more new one-shot sounds**, same theme (every boss moment gets its own identity instead of reusing the normal room game's):
- `PlayBossSpreadShotSound` — channel 3, fires once when `BOSS_VARIANT_TRIPLE_SHOT` adds its 2 diagonal bullets (`Boss.c FireAtPlayer`), distinguishable by ear from the single aimed shot (channel 1).
- `PlayBossArenaDamageSound` — replaces `PlayPlayerHitSound` in `BossRunPlayer.c`/`BossFightCollision.c` only (the normal room game keeps the original). Heavier/lower, channel 1 (shares the bassline — a hit briefly ducking the beat reads as impact, not a glitch).

Verified: full `make build_gb BUILD_TYPE=Debug` compiles clean, no new warnings (one nested-comment warning caught and fixed — a `res/music/*.uge` path in a comment looked like a comment-close to the compiler). BGB relaunched fresh each time (killed old process + deleted `bgbrecovery.sna` before each relaunch — see [[reference_ready_gamer_build]] on why that file causes stale-state confusion otherwise).

**Nothing committed.** On top of `9cbe8d6`: the 2026-09-09 corner-collision-margin + `DEBUG_START_ROOM=3`/`DEBUG_START_COINS=50` (unrelated, still sitting there), plus today's `StateMenu.c` debug-boss-run wiring and the full sound/music work across `SoundEffects.c/.h`, `Boss.c`, `BossRunPlayer.c`, `BossFightCollision.c`, `StateBossRun.c`, `StateBossFight.c`. **Not yet played/heard by the user** — next step is playtesting both music loops + all the action/damage/spread-shot cues live and tuning by ear (tempo, note choices, and the channel-contention tradeoffs are all first-pass placeholders).

## Session 2026-09-14: boss sound design (4 new cues) + debug boot moved to the auto-scroll level

**Debug boot redirected** (`src/ZGBMain.c`): `next_state` global now boots straight into `StateBossRun` instead of `StateMenu` — tagged `// DEBUG`, revert to `StateMenu` before shipping. `StateGame.c`'s own `DEBUG_START_ROOM=3`/`DEBUG_START_COINS=50` untouched (irrelevant to this boot path since the menu — and therefore `StateGame` — isn't reached by default anymore, but still reachable via `StateWin`/`StateGameOver` → menu → START).

**4 new sound cues, all in `src/systems/SoundEffects.c` / `include/SoundEffects.h`, same raw-NRxx `PlayFx` style as every existing sound (no `.uge`/tracker music involved — these are SFX, not composed tracks):**

1. `PlayBossEntranceMelody`/`UpdateBossEntranceMelody` — 4-note ascending fanfare, faster tempo than the door melody, played once in `StateBossRun.c START()`, ticked from its `UPDATE()`. The "get hyped for this level" cue.
2. `PlayBossMoveSound` — quiet low blip, `Boss.c`'s `ChaseStep()`, fires each time the boss actually steps.
3. `PlayBossShootSound` — lower/harsher than the player's own screw-shot, with a downward pitch sweep (`NR10`) for a "growl" — `Boss.c`'s `FireAtPlayer()`.
4. `PlayBossSwordSound` — noise-channel "shing", `Boss.c`, right when `STATE_SWORD_ACTIVE` begins (hitbox spawn point).
5. `PlayBossDefeatMelody` — 3-note descending melody, `Boss.c`'s `HandleDeath()` (fires for both the main boss splitting and a split copy's final death — not distinguished, kept simple).
6. `PlayBossAmbiencePulse` — quiet periodic pulse (not a held/sustained tone — see the function's header comment for why: the arena's other SFX already cycle through all 4 channels constantly, so a continuously-held channel would just get cut the first time anything else fires and never resume). Called from `StateBossFight.c UPDATE()` on its own ~1.5s timer (`ambience_timer`/`AMBIENCE_PULSE_INTERVAL`), same pattern as `StateBossRun.c`'s existing `bullet_timer`.

All values (pitch, duty, envelope, timing) are first-pass placeholders picked for rough distinctiveness, same as every other sound in this file historically — **tune by ear in BGB**, not locked in.

Verified: full `make clean` + `make build_gb BUILD_TYPE=Debug` compiles clean, no new warnings vs. the pre-existing baseline (only the already-documented overflow/optimizer notices). BGB launched on the fresh ROM — **not yet played/heard by the user**, next step is playtesting the 4 cues + entrance fanfare + ambience pulse live and tuning by ear.

**Nothing committed.** Working tree now has, on top of `9cbe8d6`: the pre-existing `WALL_COLLISION_MARGIN` + `DEBUG_START_ROOM=3`/`DEBUG_START_COINS=50` from 2026-09-09 (still uncommitted, unrelated to today), plus today's `next_state` redirect and the 4 new sound cues (`SoundEffects.c/.h`, `Boss.c`, `StateBossRun.c`, `StateBossFight.c`).

## Session 2026-09-09: corner-collision forgiveness (kept), enemy pathfinder (tried, reverted) — resume here

**Working tree right now — only 2 uncommitted changes, both intentional, nothing else:**
- `src/ZGBMain.c`: `WALL_COLLISION_MARGIN` (3px) added to `CheckEdgeMapCollision` — insets the collision box on all 4 sides so the player/enemies don't need pixel-exact alignment to turn a corner in a tight corridor. Room collision only (`SafeTranslateSprite`/`EnemyMoveWithWallAvoidance`); BossRun/BossFight untouched. **User confirmed working** ("si, todo bien con el tema de las 4 esquinas, todo funciona").
- `src/states/StateGame.c`: `DEBUG_START_ROOM` 0→3, `DEBUG_START_COINS` 0→50 — debug scaffolding for testing room3/map4 directly. **Revert to 0/0 before shipping** — same "revert before ship" bucket as every other `DEBUG_START_*` in this file.

### Enemy pathfinding — built, fought a real multi-layer bug, reverted at user's request

User asked for a real pathfinder (enemies route around walls instead of dumb X/Y-priority chase + local avoidance). Built a shared per-room "flow field" (BFS from the player's tile, cached in WRAM, read by all 5 enemy types) in a new `src/systems/PathField.c` + `include/PathField.h`, pinned to ROM bank 7. **Fully reverted per explicit request** ("quitemos todo el pathfinder... me cansé", "tiene pegas de fps") — both files deleted, all hooks removed from `ZGBMain.c`/`StateGame.c`/the 5 virus sprite files (`git checkout --` restored those 5 to HEAD exactly). If this is revisited, don't restart from scratch — the design and every bug below are worth reusing.

**Three real, distinct bugs found in one debugging arc, each masking the next (same "PC executing garbage in bank 3" symptom every time, very confusing to disentangle):**

1. **`UINT8` loop counter compared against a bound that can be 900.** `UpdatePathField`'s full-array reset loop (`for (i = 0; i < total; i++) pf_dist[i] = ...`) used `i` declared `UINT8`, shared with an unrelated 4-iteration loop elsewhere in the same function. `total` can be up to 900 (`PATHFIELD_MAX_TILES`, for map5) — a `UINT8` never reaches 900, wraps at 256, infinite loop. Real, would have hit on literally the first frame after room load. Fix: separate `UINT16` counter for that loop.

2. **[[reference-gb-bank-switch-hazard]] — the big one.** A file's own code cannot safely `PUSH_BANK` to a *different* bank if that file itself lives in a switchable bank (not HOME) and needs to keep executing its own subsequent code afterward — the instant the hardware bank flips, the caller's own code (physically stored in that same $4000-$7FFF window) becomes unreachable, and the CPU runs garbage from the new bank instead. Hit this twice: once inside `PathField.c` itself (bank 7) doing its own tile reads via `PUSH_BANK(BANK(map4))`, and again one level up when callers in *their own* switchable banks (`StateGame.c`/bank 2, the 5 virus files/bank 3) did manual `PUSH_BANK(7)/UpdatePathField()/POP_BANK` instead of using `BANKED`. Full writeup + the fix (tile reads must go through a HOME-resident function like `GetRoomTileFromTable`; cross-switchable-bank calls must use SDCC's `BANKED` keyword, never manual `PUSH_BANK` from a non-HOME caller) is in the reference memory — read that before ever adding a new pinned-bank file with cross-bank calls again.

3. **BGB's `bgbrecovery.sna`** (in `ZGB_extracted/ZGB/env/bgb/`) auto-saves an emulator snapshot and appears to reload it on next launch — repeatedly force-killing BGB (`Stop-Process -Force`) between test builds made several rounds of testing look at a stale frozen snapshot instead of the actual new build, wasting real time before this was caught. Delete that file (or close BGB cleanly) before trusting a "still crashes" report during any future debugging session that involves relaunching BGB many times in a row.

**Debugging method that actually worked, once tried:** stop guessing at banking theory, bisect empirically. Built with `make build_gb` (compiles without launching BGB — much faster iteration than `make run`, which blocks until BGB is closed) and launched BGB manually (`ZGB_extracted/ZGB/env/bgb/bgb.exe bin/READY_GAMER_Debug.gb &`) so I could kill/relaunch without waiting on `make`. Stripped `InitPathField`'s body down to nothing, confirmed the game ran fine, then added pieces back one at a time until the exact line reintroduced the crash. This narrowed 3 build-and-guess round trips into 1 short one — start here next time instead of theorizing first.

**Not yet re-attempted**: reintroducing the pathfinder with all 3 bugs fixed *and* the empirically-untested StateGame.c-level `BANKED` path (the very last build, with real `BANKED` end to end, actually got far enough to render the game and move around — the "se cae" report that triggered the final revert was an FPS/stutter complaint from the user being tired of testing, not a confirmed fresh crash; worth a clean re-test before assuming it's still broken).

## Done earlier — boss run physics

Boss map player collision and invincibility (`BossRunPlayer.c` + `ZGBMain.c` boss helpers only). **Room maps 0–4 unchanged** (`SafeTranslateSprite` / `SpritePlayer.c` untouched).

### Normal movement (no invincibility)

- Per-axis movement each frame (same pattern as `SpritePlayer.c`) so **diagonal input slides** along walls instead of stopping completely.
- `BossRunCheckCollision` uses **edge-sweep** (like `CheckEdgeMapCollision`) instead of four-corner-only checks — harder to embed inside interior walls when walking voluntarily.
- Camera crush: if `x < scroll_x`, clamp to `scroll_x` (unless invincible or pending wall-push).

### Invincibility (any source: crush, bullet, respawn)

- **`BossRunTranslateSpritePhasing`**: pass through **interior** wall tiles in **all directions** (↑↓←→).
- **Ceiling/floor rows** (map tile rows `0` and `BOSSRUN_MAP_TILES_H - 1`) **always block**, even while invincible — player stays inside the vertical corridor.
- No crush damage while i-frames active (`invuln_before > 0` for movement; overlap ignored for damage).
- While invincible, camera clamp skipped so player can move left of scroll edge to escape crush.

### When invincibility ends

- If still overlapping an **interior** solid → set `CD_PENDING_WALL_PUSH` (reuses `CD_PLAYER_ELECTRIC` slot in boss player only).
- Each frame while pending: `PushForwardOutOfWall` — increment **x** (scroll/right direction) until clear or limit (`scroll_x + SCREENWIDTH`, max 80 steps).
- No extra crush damage during pending push.
- Crush damage (`BossRunTakeDamage`) only when not invincible, not pending push, and overlapping any solid (`IsOverlappingSolid(..., 0)`).

### Key symbols

| File | Role |
|------|------|
| `include/BossRun.h` | Map size constants; `BossRunTranslateSprite` / `BossRunTranslateSpritePhasing` |
| `src/ZGBMain.c` | `BossRunTileBlocked`, `BossRunEdgeBoundaryBlocked`, shared collision + translate |
| `src/sprites/BossRunPlayer.c` | Player clone, `CustomTranslateSprite`, overlap tests, push logic |
| `src/states/StateBossRun.c` | Auto-scroll, bullet spawns, win at map right edge → `StateWin` |

### Refactor (same behavior)

- Single movement block (`can_phase_walls` flag).
- `IsOverlappingSolid(sprite, interior_only)` replaces duplicate wall tests.

---

## Local debug cheats (revert before release / merge)

| File | Cheat | Restore to |
|------|-------|------------|
| `src/states/StateMenu.c` | START/A → `StateBossRun` | `SetState(StateGame)` |
| `src/states/StateGame.c` | `DEBUG_START_COINS 100` | `0` |
| `include/SpriteData.h` | `PROJECTILE_DAMAGE_* 10` | `1` / `2` |

`DEBUG_START_ROOM` and `DEBUG_START_ELECTRIC` already at `0`.

---

## Game flow (correct)

`StateMenu` → `StateGame` (5 rooms) → portal on **map5** → `StateBossRun` → reach end of **mapboss** scroll → `StateWin` (raffle code `RGGBA26`) → menu.

---

## Boss run content — still open

Map **mapboss** is a long (240×18 tiles) mostly open horizontal corridor. Physics work; **level design / theme** not done.

Design direction discussed (video game store theme — Ready Games):

- Frame as **store aisle run** (clearance aisle / rush to counter / back room backup).
- Break 240 tiles into **8–12 rhythmic sections**: shelves/pillars, box stacks, kiosk alcoves, narrow checkout lanes.
- Tune `CameraDriver` speed + `BULLET_SPAWN_INTERVAL` per section before adding new mechanics.
- Optional later: coin pickups, static virus in alcoves, bullet patterns, CRT tile “boss face” (no new boss sprite required).

See conversation 2026-08-27 for full idea list (concepts A/B/C).

---

## Build

- After `include/` changes: `clean.bat` then `build_Debug.bat`.
- Output: `bin/READY_GAMER_Debug.gb` (BGB launches via `make run`).

---

## Next steps

1. Revert debug cheats when done testing boss map layout.
2. Paint **mapboss** sections in GBMB (geometry + store-themed tiles).
3. Commit boss physics + docs (cheats optional separate commit or reverted first).
4. Smoke-test one normal room after reverting cheats (movement, doors, portal map5).
5. Delete junk untracked file `I,len(d))+t+d+struct.pack(` if still present.

---

## Session addendum (2026-09-02, mandrix) — boss-fight arena after the corridor


Living status file, not committed history. Read this first each session to know where things stand and how to resume. Stable build/architecture docs are in `CLAUDE.md`; business context in `CONTEXT.md`.

## Stopped here (2026-08-26) — resume point

Boss run mode is **committed** across 3 commits on `feature/map5-spawn-locks` (all local only, still not pushed, on purpose, per explicit request): `a21752e` (base mode), `dcf6f50` (vertical-collision fix), `345fb3c` (debug HUD removal). Working tree matches `345fb3c` exactly — nothing else uncommitted except this file.

**User verdict on current tuning: good as-is.** Auto-scroll speed and bullet aim/timing both feel right — no tuning changes made this pass, just bug fixes and cleanup.

**Bullet reskin tried and reverted (2026-08-25)**: designed a "flying game cartridge" sprite for `BossBullet` (store-themed, replacing the reused `bomb` graphic) — previewed as an artifact, built into `src/assets/bossBulletGfx.c`, wired into `ZGBMain.h`, tested in BGB. **User rejected it in-game — too big, preferred the original bomb bullet.** Reverted cleanly, nothing left behind. If a boss-themed bullet is revisited later, keep it visually smaller/simpler than the 16×16 cartridge attempt — that was the specific complaint, not the concept.

**Fixed (2026-08-26, `dcf6f50`): invincibility let the player phase through walls on any axis, not just the one that needed it.** User noticed that after taking a hit, they could move up/down through walls with no collision at all. Root cause: `CustomTranslateSprite` (`BossRunPlayer.c`) bypassed `BossRunTranslateSprite` entirely while `CD_INVINCIBILITY > 0`, on both x and y — but the only reason that bypass exists is to let the player escape being crushed against a wall by the camera's x-clamp (`UPDATE()` forces `THIS->x = scroll_x` with no collision check). The camera never force-moves y, so vertical movement was never actually at risk of embedding the player in a wall — the y-bypass was pure collateral, not needed by the escape mechanic. **Fix**: bypass now only applies when `x != 0` (horizontal calls); vertical calls (`x == 0`) always go through normal `BossRunTranslateSprite` collision, invincible or not. Verified in BGB by the user.

**Removed (2026-08-26, `345fb3c`): debug HUD.** The `L:%d B:%d` readout (lives, bullets fired) plus its backing `bullets_spawned` counter and `INIT_CONSOLE`/`Print.h` scaffolding are gone from `StateBossRun.c` — user confirmed bullet damage/timing feels right, no longer needed.

**Still there on purpose**: menu wiring (all tagged `// DEBUG` in `StateMenu.c`) still has **START/A → boss run, SELECT → normal game** — kept intentionally so testing stays reachable; this is the one remaining piece to swap back (`SetState(StateGame)` on START/A, drop the SELECT branch) before actually shipping.

**Next session should start with**: either more playtesting/polish (boss-themed bullet art take two if wanted, section layout variety), or — if the user says this is ready — revert the `StateMenu.c` debug wiring and commit that as the final pre-ship cleanup.

## NEW: experimental auto-scroll "boss run" mode (StateBossRun) — first pass, uncommitted

Separate game mode from the room/dungeon-crawler system entirely — auto-scrolling corridor, dodge bullets, reach the end to win. **Current menu wiring (tagged `// DEBUG` in `StateMenu.c`, swap back before ship): START/A → boss run, SELECT → normal game** (swapped from the first pass — user wants normal boot behavior preserved, just START repurposed for testing).

**Architecture decisions made this pass (see full reasoning in conversation history if resuming):**
- New `State` (`StateBossRun`), not a room — auto-scroll is a fundamentally different paradigm (camera not player-driven) than the room system's doors/spawns/exploration model.
- **Camera**: no new scroll-driving code at all. A new invisible sprite, `CameraDriver` (`src/sprites/CameraDriver.c`), just increments its own `x` every 2 frames and is set as `scroll_target`. The engine already auto-calls `RefreshScroll()`/`MoveScroll()` off of `scroll_target->x` every frame (`SpriteManager.c:231`) — this is the *exact* mechanism that already drives normal player-follow camera in `StateGame`, just pointed at a sprite that walks itself forward instead of the player. Zero risk to the existing camera code.
- **No boss body sprite.** The GB only allows 20 active sprites total and a sprite is max 16px tall — covering the full 144px screen height would eat ~9 sprite slots just for the boss's body. For this first pass the boss is implied, not drawn: only its bullets (`BossBullet`) exist as sprites, spawned off-screen-right by a timer in `StateBossRun.c`, at the player's current Y (a simple "aimed" horizontal shot). A real boss graphic can be added later (needs someone to draw it in GBTD — Claude can't create new pixel art) or drawn via the Window layer (screen-locked by hardware, would cost zero sprite slots) — not attempted yet, too risky to get right without visual verification in one pass.
- **No new art** anywhere: `BossRunPlayer` reuses the normal player graphic, `BossBullet` reuses the `bomb` graphic, `CameraDriver` reuses `spawner` (never actually visible — parked at y=200, below the map).
- **Collision**: a small self-contained collision helper (`BossRunTileBlocked`/`BossRunTranslateSprite` in `ZGBMain.c`, declared in new `include/BossRun.h`) — deliberately *not* sharing the room system's `CheckEdgeMapCollision`/partial-brick code, since mapboss only ever uses tiles 0 (floor) / 1 (wall), no partial bricks. Keeps the two collision systems independent so tuning one can't regress the other.
- **Lose condition (revised — user wanted full parity with the normal player):** `BossRunPlayer.c` is now a near-complete copy of `SpritePlayer.c` — same health/lives/respawn/invincibility-flash/shooting, reusing the shared `player_lives` global (reset to 2 in `START()`). Only real difference: movement goes through `BossRunTranslateSprite` (mapboss's collision) instead of the room system's `SafeTranslateSprite`. `TakeDamage` is copied too but renamed `BossRunTakeDamage` (avoids a duplicate-symbol link error against `SpritePlayer.c`'s own `TakeDamage`) — `BossBullet` calls it on hit, same as `Bomb.c` does for the normal game. It calls `SetState(StateGameOver)` itself once lives hit 0, so `StateBossRun` doesn't need to poll for death anymore. A 1-line HUD (`Lives:%d`) was added to `StateBossRun.c`.
- **Win condition**: reuses the engine's own scroll clamp — `ClampScrollLimits()` (`Scroll.c`) already caps `scroll_x` at `map_width - SCREENWIDTH`. `StateBossRun`'s `UPDATE()` just checks `scroll_x >= that cap` each frame → `SetState(StateWin)` (existing win screen, reused as-is). No separate timer/distance tracking needed.

**The map (`res/mapboss.gbm`) is generated, not hand-painted in GBMB.** Built by a Python script (not committed — lived in the scratchpad this session, would need rebuilding from scratch if wanted again) that directly writes the `.gbm` binary format (same format reverse-engineered earlier this session for the map5 work) by cloning `map5.gbm`'s non-tile-data chunks verbatim and swapping in new width/height + a generated tile grid. Current map: 4 hand-designed 20×18 "sections" (single/weave/gated wall-pillar patterns, each with a guaranteed 2-tile floor buffer on every edge so any section can follow any other with a clean seam), repeated 3× = 240×18 tiles (1920×144px) — about a minute at the current scroll speed. **This is a first-pass demo layout, not final level design** — the section patterns, repeat count, and scroll/bullet-timing constants (`AUTOSCROLL_FRAME_DIVIDER` in `CameraDriver.c`, `BULLET_SPAWN_INTERVAL` in `StateBossRun.c`, `BOSSBULLET_SPEED` in `BossBullet.c`) are all easy to retune once playtested.

New Makefile rule pins `mapboss.gbm` into ROM bank 5 (same manual-bank-pinning pattern as the other 5 maps, for build stability — see `src/Makefile`).

Verified: full clean rebuild compiles and links with no errors and no new warnings.

**Playtest round 1 found 2 real bugs, both fixed:**
1. **Top wall row was invisible.** `CameraDriver` sits at `y=200` (kept off the visible 144px map so its placeholder sprite is never seen) — but the engine's auto-follow camera also centers `scroll_y` on `scroll_target->y`, dragging the vertical scroll away from 0 and clipping the top row out of view. Root cause was specifically `INIT_CONSOLE`'s side effect of setting `scroll_h_border` (reserves space under a HUD window — irrelevant here since mapboss is exactly one screen tall) which let that drag actually move the clamp ceiling. Fixed by forcing `scroll_h_border = 0` and `scroll_y = 0` every frame in `StateBossRun.c`.
2. **Bullets never damaged the player.** `BossBullet.c`'s off-screen cleanup check compared the bullet's x against `scroll_x - 16` — but the player naturally sits near the camera's left edge in an auto-scroller (keeping pace with it), so bullets were self-deleting for being "off-screen" right in the same zone the player occupies, before the collision check below it ever ran. Fixed by making that check an absolute `THIS->x < 32` (pure underflow safety net for the first couple seconds of the level) instead of scroll-relative — the lifetime timer is what actually cleans up bullets in normal play.
3. **Removed the 1-line `Lives:%d` HUD** — the GB window layer always draws on top of the bottom screen row regardless of `scroll_h_border` (that variable only affects the *background* scroll clamp, not window draw order), so it was permanently covering the bottom wall. No on-screen lives indicator for now; lives are still tracked and functional, just not displayed. Revisit with a HUD approach that doesn't eat a map row if this needs solving later (e.g. redesign mapboss so row 17 is dedicated HUD space and the real bottom wall is row 16).

**Playtest round 2**: top wall confirmed fixed. Bullets still weren't damaging — added a temporary debug HUD (`L:%d B:%d`, `StateBossRun.c`, tagged DEBUG) which revealed the real cause: bullet count was climbing normally (spawning fine) but lives never dropped, and the player sprite wasn't even visible on screen anymore. **Root cause**: nothing was stopping the player from falling behind the auto-scrolling camera — if you don't actively move right to keep pace, the camera leaves you behind indefinitely (nothing enforces "keep up" in an auto-scroller by default), eventually scrolling you off-screen-left, far outside any bullet's spawn-to-lifetime travel range. **Fixed**: `BossRunPlayer.c` now clamps its own x to never go left of `scroll_x` (the camera's left edge acts as a moving wall, standard for the genre) — added right after the movement-key handling in `UPDATE()`.

Debug HUD (`L:%d B:%d`) still in the ROM as of this fix, not yet removed — keep it until bullet damage is confirmed working with the camera-clamp fix in place.

**Playtest round 3**: camera-clamp fix caused a new problem — clamping `x` to `scroll_x` bypasses collision entirely, so if a wall pillar happens to sit right at the camera's edge, the player gets shoved *into* it and stuck (can't move, no valid direction is collision-free). Fixed in `BossRunPlayer.c`:
- `CustomTranslateSprite` now lets the player move through walls entirely while `CD_INVINCIBILITY > 0` — otherwise there'd be no way to escape a crush before getting hit again the instant invincibility ran out.
- New `IsCrushedByWall(sprite)` checks the 4 corners of the sprite's current position against `BossRunTileBlocked` — if any are inside a wall (only possible via the scroll_x clamp forcing position without a collision check), calls `BossRunTakeDamage(THIS)` every frame. Safe to call every frame — `BossRunTakeDamage` no-ops while already invincible, so this naturally becomes "hit once, then a grace window to actually get clear."

## Quick start

- Build+run (reliable path, `.bat` wrapper fails when backgrounded):
  ```
  cd src
  export ZGB_PATH="E:/Users/Alejandro/Opal/Game-Boy/ZGB_extracted/ZGB/common"
  "$ZGB_PATH/../env/make-3.81-bin/bin/make" run BUILD_TYPE=Debug
  ```
  Run this in the background — it also launches BGB on success and sits there until BGB closes.

## Room 6 draft → became the new map5 (room index 4 / "nivel 5") — DONE this session (uncommitted)

Per the user's call: **map6 replaces map5 entirely** rather than becoming a 6th room. `res/map5.gbm` (old 40×18 room) was deleted; `res/map6.gbm` (the 50×18 draft) was renamed to `res/map5.gbm`. No `IMPORT_MAP`/C-symbol changes were needed anywhere — the code already imports/references `map5` for room index 4, and that symbol now resolves to the new file's content. `room_count`/`MAX_ROOMS` stay at 5.

Door/spawn/player/portal coordinates weren't available as GBMB grid numbers — they only existed as hand-drawn colored blobs over a GBMB screenshot (`image copy 5.png`: red=spawn, black=door, green=player start). Extracted them **without eyeballing pixels**:
- Calibrated the screenshot's grid geometry using GBMB's own blue cursor-highlight (`[49,17]` shown in its status bar) as a ground-truth anchor — confirmed exact (16px/cell, origin at pixel (61,110)).
- Parsed `res/map6.gbm`'s binary tile data directly (chunk format from `ZGB_extracted/ZGB/tools/gbm2c/gbm2c.cpp`, documented in [[reference-ready-gamer-map-editing]]) to get the *real* wall layout — not a redrawn approximation — and ran BFS reachability from the player-start cell to deduce which spawns are reachable at room-load (unlocked by default) vs. only reachable once a specific door opens (locked, then that door's `unlock_spawn_indices` list names them). One coincidental cross-check: a stray leftover tile (ID 5, clearly an accidental paint) sat at exactly the pixel-derived door-D3 position — independent confirmation the calibration was right. That stray tile was patched to floor (ID 4) directly in the `.gbm` binary before the rename (byte-patch, not a redraw — object CRCs in this file are all 0/unused so this doesn't corrupt anything GBMB would check).

Final layout (`Rooms.c`, room index 4 / `map5`):
- Player start (3,15) tile → pixel (24,120).
- 3 doors, cost 10 each: D1 (26,1)→(208,8), D2 (38,1)→(304,8), D3 (12,15)→(96,120).
- 9 spawn points; D1 gates 3 of them (`room4_door0_unlocks = {3,5,8}` = S4/S6/S9, indices into `room4_spawns[]`), the other 6 start unlocked. D2/D3 don't gate any spawn (confirmed with the user) — they gate the path to the exit portal instead, which needs all 3 doors open to be reachable (verified by BFS).
- Portal (46,15) → pixel (368,120). No coin/electricity pickups in this room (confirmed with the user).
- Full reasoning is also left as a comment block directly above `room4_doors` in `Rooms.c`.

This is also what pushed the spawn-locking feature (below) from "8 slots, all-unlocked-by-default" to actually needing **per-spawn initial lock state**: `SpawnPointPlacement` gained an `initially_locked` bool (the 12 spawn points in rooms 0-3 got `0` appended to stay opt-out), and `MAX_ROOM_SPAWN_POINTS` went from 8→10 (map5 alone uses 9).

**Testing scaffolding added per explicit request**: `StateGame.c`'s `DEBUG_START_ROOM` was changed `2→4` so the game boots straight into the new map5 for testing — this stacks with the *pre-existing* uncommitted debug scaffolding below, don't lose track of which is which when reverting for ship.

Verified: full Debug rebuild compiles clean (no new warnings), all 13 placed coordinates validated against the real tile data (none land on a wall tile #1), BFS-confirmed the level is completable (portal reachable once all 3 doors are open, no spawn is permanently unreachable).

## Map editing workflow (established this session)

- **GBTD** edits one 8×8 tile at a time. **GBMB** lays tiles into the full map (set width/height at `File → New` or `Map Properties`) — don't confuse the two when someone says "the canvas is too small."
- Old Win32 app tiny on modern displays: right-click the `.exe` → Properties → Compatibility → **Change high DPI settings** → check **Override high DPI scaling behavior** → set to **System**.
- **Tile IDs 1/2/3 in any `.gbm` are reserved for collision**, not just visuals (`ZGBMain.c`: `TILE_FULL_BRICK=1`, `TILE_PARTIAL_BRICK_1=2`, `TILE_PARTIAL_BRICK_2=3`). Whatever occupies those slots in `tiles.gbr` becomes a wall/platform automatically. Room borders must specifically use slot `#1` — that's what all 5 existing rooms do. `#2`/`#3` are half-height "partial brick" platforms, not full walls — don't use them for map edges (caught this exact mistake in map6's first draft).
- Map edges are *also* auto-clamped by the engine (`ZGBMain.c` `CheckEdgeMapCollision`, against `scroll_w`/`scroll_h`) independent of tile art — a player can never leave the map even with zero border tiles. Border art is cosmetic for the edge itself, but keep it for visual consistency with other rooms.
- `.gbm`'s binary format is fully documented in the engine's own map compiler source: `ZGB_extracted/ZGB/tools/gbm2c/gbm2c.cpp`. Chunk-based (`ObjectHeader` + typed chunks); `OBJECT_TYPE_MAP` (`0x2`) holds width/height at fixed offsets, `OBJECT_TYPE_MAP_TILE_DATA` (`0x3`) holds one 3-byte tile record per cell. Useful if a `.gbm` ever needs to be read or generated by script instead of through GBMB.

## Spawn locking — IMPLEMENTED this session (uncommitted)

Doors can now selectively lock/unlock spawn points when opened. Design:

- `Rooms.c`: `DoorPlacement` gained two fields — `unlock_spawn_indices` (array of indices into that room's `spawn_points[]`) and `unlock_spawn_count`. Default `{NULL, 0}` = door doesn't touch spawn locks at all (opt-in, so all 5 existing rooms are unaffected — verified by rebuild, no behavior change since no door defines a list yet).
- `SpawnPointPlacement` also gained `initially_locked` (bool) — whether that spawn starts locked at room load, read by `ResetRoomSpawnLocks` (called from `SpawnRoomEntities` on every room load). Existing rooms 0-3's spawn points all pass `0` (unaffected).
- Runtime lock state: `static UINT8 spawn_locked[MAX_ROOM_SPAWN_POINTS]` in `Rooms.c` (`MAX_ROOM_SPAWN_POINTS=10` — map5 uses 9, the current max; bump if a future room needs more; `GetRandomSpawnPosition`/`ApplyDoorSpawnUnlocks` silently ignore spawn indices beyond the cap rather than overflow).
- `ApplyDoorSpawnUnlocks(door_x, door_y)`: called from `SpritePlayer.c:HandleDoorInteraction` right when a door opens (before it's removed from the sprite manager, so `door_sprite->x/y` is still valid). Matches the door by world position against `room->doors[]`. If that door has a non-empty unlock list: spawns in the list become unlocked, **every other spawn point in the room gets locked** — this is a "set exclusively to this door's list" operation, not additive. Cross-bank call goes through a `*FromTable` wrapper in `ZGBMain.c` (`ApplyDoorSpawnUnlocksFromTable`), same pattern as the existing room helpers.
- `GetRandomSpawnPosition` (`Rooms.c`) now only picks among currently-unlocked spawn indices; if a misconfiguration locks all of them, it falls back to treating all as usable rather than soft-locking the wave (this is the safety net for the soft-lock bug the memory previously flagged).
- **Not implemented**: doors unlocking new monster types (that part of the original idea) — out of scope for this pass, still an open design fork, see memory.
- **Now wired to a real room**: map5 (room index 4) uses both `initially_locked` and a door unlock list — see the room-6-became-map5 section above for the concrete layout. Rooms 0-3 still don't use it (opt-out, unaffected).
- Verified: full Debug rebuild compiles clean (systems/Rooms.c, SpritePlayer.c, ZGBMain.c, states/StateGame.c — no new warnings/errors vs. pre-change baseline).

To wire a door to gate spawns, in `Rooms.c`:
```c
static const UINT8 room1_door0_unlocks[] = { 0, 2 }; // indices into room1_spawns[]
static const DoorPlacement room1_doors[] = {
    { 115, 52, 10, room1_door0_unlocks, ARRAY_LEN(room1_door0_unlocks) },
    { 115, 100, 10, NULL, 0 }, // this door still doesn't gate spawns
};
```

## Outstanding (still uncommitted)

- Lives/respawn feature (`SpritePlayer.c`, `StateGame.c`, `SoundEffects.c/.h`) — done, working, uncommitted.
- Debug scaffolding: `DEBUG_START_ELECTRIC`, `DEBUG_START_COINS`, and the x10 damage values (`SpriteData.h`) are reverted to normal as of this session. **Only `DEBUG_START_ROOM = 4` remains** (`StateGame.c`, normal value `0`) — kept on purpose so the game keeps booting into map5 for testing. Flag before committing/shipping — don't revert silently in case testing is still ongoing.
- **Playtest round 1 (user, in BGB) found real bugs, fixed:**
- **Root cause of an "invisible wall" + weird enemy movement**: `GetRoomTileFromTable` in `ZGBMain.c` had the map width **hardcoded to `MAP_TILE_W=40`** for every room's tile-index math (`index = y * MAP_TILE_W + x`) and its bounds check. That was correct while all 5 rooms were 40 tiles wide, but map5 is now 50 wide — for any row beyond row 0, the index math read tile data from the wrong offset (misaligned by 10 tiles per row and growing), producing phantom collisions (the invisible wall) and confusing enemy wall-avoidance (same collision code path, `SafeTranslateSprite`/`EnemyMoveWithWallAvoidance` per `CLAUDE.md`'s architecture notes — likely also explains the "monsters coming from the door side" report). **Fixed** to use `scroll_tiles_w`/`scroll_tiles_h` (the engine's own per-room tile dimensions, already set correctly from each map's real `MapInfo.width/height` by `InitScroll` — see `ZGB_extracted/ZGB/common/src/Scroll.c:259-260`) instead of the hardcoded constant. This bug would have hit *any* room with a non-40-wide map, not just map5 — worth remembering if a future room ever isn't 40 tiles wide again.
- S3 spawn realigned to the same row as S1/S2 (was one row lower) — user wanted the top-left 3 spawns visually in a line; moved from tile (3,3) to (3,2) in `Rooms.c`.
- Rebuilt clean after both fixes (`ZGBMain.c`, `Rooms.c`), BGB relaunched on the new ROM.

**Playtest round 2**: invisible-wall fix not independently confirmed yet, but user reported enemies still spawning from the right side — turned out to be the *original* lock design (6 of 9 spawns unlocked at start, reachability-derived) not matching what they actually wanted. Replaced with an explicit user-specified progression (overrides the reachability-based one from round 1):
- Start: only S1/S2/S3 unlocked, everything else (S4-S9) locked.
- **D3** (bottom-left, next to player start) = "first door": unlocks S5/S7/S8 (locks S1/S2/S3 back up, per the existing "replace" semantics).
- **D1** (top-left) = "second door": unlocks S4/S6/S9 (locks everything else). Same list D1 already had from round 1 — coincidence, not reused logic.
- **D2** (top-right) still doesn't gate any spawn — gates the portal path only.
Rebuilt clean, BGB relaunched again.

## Two separate BGB debugger breaks — one real, NOW ROOT-CAUSED AND FIXED, one confirmed benign

**Incident 1 — SOLVED.** Original theory (interrupts firing mid-`PushBank`) was wrong. The real cause, found via a *reliably reproducible* version of the exact same crash signature (`PC` stuck at `0x0038` running `FF`/`rst 38`) that showed up this session while building the boss-run mode: **`TakeDamage` (`SpritePlayer.c`) was called cross-bank from `Bomb.c` without the `BANKED` keyword.** Per this project's own documented gotcha (`CLAUDE.md`: "any function called across ROM banks must be declared `BANKED`... a missing `BANKED` compiles cleanly but crashes at runtime") — `Bomb.c`'s forward declaration (`void TakeDamage(Sprite* player);`) and `SpritePlayer.c`'s definition were both missing it, so a call from `Bomb.c`'s bank emitted an unsafe near-`CALL` instead of a proper far-call. This matches Incident 1's context exactly: it happened when a `BomberVirus`'s `Bomb` touched the player, i.e. `Bomb.c` → `TakeDamage`. **Fixed**: added `BANKED` to both the declaration (`Bomb.c`) and definition (`SpritePlayer.c`). This is a pre-existing bug from before this session, now fixed as a side effect of chasing its twin below — worth a changelog mention since it could have hit any player, any time a bomb or the equivalent kills them.

**Same bug, freshly introduced — SOLVED.** While building `StateBossRun`, `BossRunTakeDamage` (`BossRunPlayer.c`) was called from `BossBullet.c` — same missing-`BANKED` mistake, but this time *reliably* reproducible (every single bullet hit crashed it), which is what made root-causing Incident 1 possible. Fixed the same way: `BANKED` added to both `BossBullet.c`'s declaration and `BossRunPlayer.c`'s definition.

**Lesson for any future cross-sprite function call**: if sprite A's code calls a function defined in sprite B's `.c` file, that function needs `BANKED` on *both* the definition and every forward declaration used by callers — grep for bare `void FuncName(...)` declarations near `extern`/forward-declared cross-file sprite calls before shipping anything new that follows this pattern (`TakeDamage`-style).

**Incident 2 (confirmed benign, no fix needed)**: separately, BGB broke again with the same "accessing inaccessible VRAM" title, but this time PC was on sensible code (`ld (de),a` inside the map/scroll tile-streaming routine, writing to a BG tilemap VRAM address — matches source context `if (room_index == 4) scroll_bank = BANK(map5)`, i.e. right when entering map5). This is BGB flagging a VRAM write that landed outside vblank/hblank — on real hardware this just risks a single glitched tile for a frame, not a freeze. **Confirmed non-fatal**: user hit Run/F5 in the debugger and the game continued playing completely normally afterward (reached level 13, no further issues). Most likely explanation: map5 is wider than the other rooms, so its tile-streaming-on-room-entry has more bytes to write and can occasionally spill past the vblank window. No code changed for this — not worth chasing unless it starts causing visible glitches during actual play (not just a debugger trap).

## Root cause found and fixed: UINT8 truncation on spawn X coordinates

The "enemies keep spawning at S1/S2/S3 no matter what door I open" reports were real — root-caused via a temporary live debug HUD readout (`DBG L:### Idx:#` on a 3rd console line — `GetSpawnLockBitmask(FromTable)` / `GetLastSpawnIndex(FromTable)` added to `Rooms.c`/`Rooms.h`/`ZGBMain.c`, printed from `StateGame.c`'s `UPDATE()`) plus an isolation experiment (temporarily set only *one* spawn's `initially_locked=0` at a time — e.g. only S4 unlocked, confirmed via `L`, but the enemy still visibly spawned right next to S3).

**Root cause**: `GetRandomSpawnPosition(UINT8* x, UINT8* y)` in `Rooms.c` returned the spawn's pixel X through a `UINT8*` (max 255) — `*x = (UINT8)spawn->x;`. map5 is 400px wide (50×8); S4 and S9 both sit at x=280 (35×8). `280 & 0xFF = 24`, which is **exactly S3's x** (3×8=24) — so every enemy meant for S4/S9 silently reappeared at S3's column instead. This bug predates this session but never had a chance to trigger before — every room prior to map5 was ≤320px wide (all spawn x < 256).

**Fix**: `GetRandomSpawnPosition`/`GetRandomSpawnPositionFromTable` (`Rooms.c`/`Rooms.h`/`ZGBMain.c`) now take `UINT16*` instead of `UINT8*`; `StateGame.c`'s `SpawnEnemies()` local `UINT8 x, y;` widened to `UINT16 x, y;` (its target, `SpriteManagerAdd`, already takes `UINT16`). Rebuilt clean. The door/spawn lock logic itself was correct the whole time — proved by 3 exact bitmask matches (504/303/215) before this was found; don't re-litigate that part if this resurfaces.

**Debug scaffolding cleaned up (this session, after the fix was confirmed):**
- Removed: 3-line console window (back to `INIT_CONSOLE(font, 2)`, both call sites in `StateGame.c`), the `DBG L:### Idx:#` HUD line, and its backing functions (`GetSpawnLockBitmask`/`GetLastSpawnIndex` + `*FromTable` wrappers — deleted entirely from `Rooms.c`, `Rooms.h`, `ZGBMain.c`, not just left unused).
- Reverted to normal balance: `DEBUG_START_ELECTRIC` → `0`, `DEBUG_START_COINS` → `0` (`StateGame.c`), `PROJECTILE_DAMAGE_NORMAL`/`_ELECTRIC` → `1`/`2` (`SpriteData.h` — the x10 debug cheat is gone).
- **Kept on purpose, per explicit request**: `DEBUG_START_ROOM = 4` — game still boots straight into map5. Still debug scaffolding (→ `0` before a real ship), just intentionally left in for continued map5 testing.
- Required a full `make clean` + rebuild (both `SpriteData.h` and `Rooms.h` under `include/` changed) — compiled clean, no new warnings, BGB relaunched.

---

## Session addendum (2026-09-02, later same day) — boss-fight arena "blank screen" bug hunt, STOPPED MID-INVESTIGATION

**Status: unresolved, in progress. Resume here next session — do NOT assume anything is fixed.**

### Where things stand right now (uncommitted, nothing pushed)

All the boss-fight-arena feature code from earlier today is in place (Boss.c, BossBulletAimed.c, BossSwordHitbox.c, BossFightPlayer.c, BossFightCollision.c, StateBossFight.c, bossGfx.c, BossFight.h) and is functionally complete per the original plan. **But `src/states/StateBossFight.c` right now has a TEMP bisection stub in it** — `START()` spawns only a `CameraDriver` (off-screen by design) instead of the real player+boss+HUD, and `UPDATE()` is a no-op. **Restore it** (the real version is quoted in full further down this note) before doing anything else with this feature.

### The bug

Entering `StateBossFight` with **zero sprites spawned** renders correctly — border walls and floor visible, confirmed by the user ("veo el borde correctamente"). The moment **any sprite** gets added via `SpriteManagerAdd`/`SpriteManagerAddEx` in that state's `START()`, the **entire screen goes blank/white — including the previously-correct border** (not a BGB debugger crash this time, no trap, just a blank LCD). Confirmed with three different sprite types:
- `BossFightPlayer` (this feature's own sprite) → blank.
- `TankVirus` (existing, unrelated, proven-elsewhere sprite) → blank. **But this test is contaminated**: `TankVirus.c`'s `UPDATE()` dereferences `scroll_target->x` unconditionally, and `StateBossFight` deliberately sets `scroll_target = NULL` (static arena, no camera) — so this sprite would null-deref crash on its own merits regardless of the real bug. Don't treat this result as clean evidence of "any sprite breaks it" — only `BossFightPlayer`'s result is clean (it never touches `scroll_target`).
- `CameraDriver` (forces its own `y = 200`, off-screen, in `START()`) → **test launched, build succeeded, BGB running — result never came back, user went to sleep mid-test.** This is the literal next thing to ask about next session. It matters because CameraDriver still goes through `SpriteManagerLoad` (VRAM tile loading) but — since it immediately teleports itself off-screen — never actually goes through `DrawSprite`'s `move_metasprite` on-screen render path. If this ALSO blanks the screen: the bug is in sprite *loading*, not rendering. If it does NOT blank (looks like the empty-map case): the bug is specifically in *drawing* a sprite on screen in this state.

### What's been ruled out (confirmed, don't re-litigate)

- **Not the pixel art**: swapped `Boss`'s gfx to `tankVirus` (proven-good existing sprite) via `ZGBMain.h`'s `_SPRITE_DMG(Boss, ...)` line — still broke. `bossGfx.c` itself is very likely fine.
- **Not `Boss.c`'s gameplay logic**: chase movement, bullet spawn, and touch-damage were each individually commented out (grep for `TEMP bisection` if any residue remains — should all be gone now, file was fully restored) — still broke with all three disabled.
- **Not a stale build cache** (though this WAS worth checking and should stay part of the routine): discovered mid-session that **`make clean` was never run despite editing `include/BossFight.h` and `include/ZGBMain.h` repeatedly** — this project's own `CLAUDE.md` documents that a plain rebuild doesn't pick up `include/` changes. Ran a full `make clean BUILD_TYPE=Debug` + rebuild — confirmed no compile errors — but the blank-screen bug persisted identically after a genuinely clean build. So it's a real bug, not stale objects. **Still, make it a habit**: run `make clean` after any `include/` edit for the rest of this feature's life, don't skip it again.
- **Real, legitimate bugs found and fixed along the way** (keep these fixes, they're correct regardless of whether they're THE cause of the blank screen):
  1. `InitBossFightScroll` (`src/systems/BossFightCollision.c`) originally used `memcpy()` to copy `mapbossarena`'s 360 tile bytes into a WRAM cache while a ROM bank was manually paged in via `PUSH_BANK`. Calling a library routine while a bank is manually swapped is unsafe if that routine's own code isn't in the fixed $0000-$3FFF region — replaced with a hand-written indexed-loop copy (pure inline loads, no `CALL`). This was necessary to stop movement collision checks (`BossFightTileBlocked`, called every frame, many times) from repeatedly `PUSH_BANK`-ing into mapbossarena's ROM bank on every single tile check, which was producing BGB debugger traps (PC stuck executing garbage, `rom=6`) — that specific crash class is gone since this fix; it just turned out there was a second, different bug (this "blank screen" one) underneath it that the crash had been masking.
  2. `BossSwordHitbox.c`: sword-hitbox position math used unsigned (`UINT16`) subtraction that could underflow near the top/left walls (`boss_y - 16` etc.), teleporting the hitbox miles off-map instead of just clamping to the wall. Fixed with signed arithmetic + a 0-floor clamp.
  3. `BossBulletAimed.c`: the near-miss swap looked up its firing boss purely by slot index (`boss_slots[]`), which could theoretically hijack a *different* boss into the sword sequence if the original died and phase-2's split reused its slot while the bullet was still in flight. Fixed by caching the firing boss's `unique_id` at spawn and re-verifying it before acting.

### Leading theories not yet tested (start here next session)

- **VRAM tile-address overlap between background and sprite tiles.** `mapbossarena` is the *first-ever* map in this project that's exactly screen-sized (20x18 = 160x144, zero scroll margin) with no scroll_target. Every other map/state that successfully mixes background tiles and sprite tiles has scrolling margin; this is uncharted territory. `SpriteManagerLoad` (`ZGB_extracted/ZGB/common/src/SpriteManager.c`) allocates sprite VRAM tiles starting at index 128 and counting *down*; background tiles are loaded starting at index 0 counting up (`ScrollSetTiles`/`last_tile_loaded`). Worth actually computing how many background tiles `tiles.gbr` (the shared tileset mapbossarena also uses) occupies, and whether GB's signed vs. unsigned tile addressing modes could make a background tile alias the same VRAM bytes as an early sprite tile in this specific case. Have not verified the actual addressing mode this engine's `set_bkg_data`/background rendering uses (signed $8800 method vs unsigned $8000) — that determination would either confirm or kill this theory outright.
- **Something about `scroll_target == NULL` specifically breaking `DrawSprite` or a related call once a real (non-off-screen) sprite needs to actually render**, as opposed to breaking sprite *loading*. The CameraDriver test (pending, see above) is designed to distinguish exactly this.
- Have NOT yet tried: opening BGB's VRAM/tile viewer panel (visible in earlier crash screenshots) while paused on the blank screen, to directly inspect what actually landed in VRAM instead of continuing to guess blind via bisection. This is probably the fastest path to ground truth next session — ask the user to open it and describe/screenshot what's there instead of another build-and-guess round trip.
- Have NOT tried: setting `scroll_target` to a real (even dummy, off-screen) sprite instead of `NULL`, matching how every OTHER working state does it, as a diagnostic — if that alone fixes it, that's the answer, even without fully understanding why NULL breaks it.

### File state reference — the REAL StateBossFight.c to restore

START() should be: HIDE_WIN; SetWindowY(144); scroll_h_border=0; last_tile_loaded=0; last_bg_pal_loaded=0; scroll_offset_x=0; scroll_offset_y=0; scroll_target=NULL; SpriteManagerReset(); InitBossFightScroll(); boss_slots[0]=NULL; boss_slots[1]=NULL; SpriteManagerAdd(BossFightPlayer, 24, 60); SpriteManagerAddEx(Boss, 112, 50, BOSS_MAIN_HP); INIT_CONSOLE(font, 1); SHOW_BKG; SHOW_SPRITES;

UPDATE() should be: iterate sprites counting type==Boss into boss_count and summing custom_data[0] into boss_hp_total; DPRINT_POS(0,0); DPrintf("BOSS:%d   ", boss_hp_total); if (boss_count==0) SetState(StateWin);

(This is also preserved verbatim in the conversation history / git diff of this session if the exact text is needed — the shape above is unambiguous enough to reconstruct byte-for-byte from the surrounding code.)

### Git state

Nothing committed this session. Branch `feature/map5-spawn-locks`, up to date with `origin` (already pulled and merged a teammate's boss-run-physics commit earlier this session — see the addendum above this one — one small conflict in `StateMenu.c` resolved by hand, kept both DEBUG shortcuts: START/A → `StateBossRun`, SELECT → `StateBossFight`). Modified: `include/ZGBMain.h`, `src/Makefile`, `src/sprites/SpriteScrew.c`, `src/states/StateBossRun.c`, `src/states/StateMenu.c`. New untracked: `include/BossFight.h`, `res/mapbossarena.gbm`, `src/assets/bossGfx.c`, `src/sprites/Boss.c`, `src/sprites/BossBulletAimed.c`, `src/sprites/BossFightPlayer.c`, `src/sprites/BossSwordHitbox.c`, `src/states/StateBossFight.c`, `src/systems/BossFightCollision.c`. Do not commit any of this until the blank-screen bug is actually fixed and the feature has been played, not just "doesn't crash."

---

## Session addendum (2026-09-04) — map4 hand-designed by the user in GBMB, wired in; found a real GB sprite-hardware limit

**Resume point for tomorrow: nothing committed. Working tree has `src/systems/Rooms.c`, `res/map4.gbm`, `src/states/StateGame.c` modified — see exact state below before touching map4/room3 again.**

### What happened, in order

1. **Procedural maze attempt (v1, then v2) — both abandoned, fully reverted.** First pass used 1-tile-wide corridors — the player (16×16 collision box, confirmed via `player.gbr.c`) could never fit, softlocking movement instantly. Second pass fixed corridor width to 2 tiles and verified with a faithful Python port of the engine's own `CheckEdgeMapCollision` (simulated real 16×16-box movement, not just "is the tile open") — this one actually worked, but the **user rejected the whole approach**: enemy AI (`EnemyMoveWithWallAvoidance`) is too dumb to navigate a tight generated maze without getting stuck on walls. Per explicit request ("mejor abre la app para que yo lo dibuje"), reverted `res/map4.gbm` and `src/systems/Rooms.c` to git HEAD (both untouched since `6b71b34`) and opened GBMB (`ZGB_extracted/ZGB/env/tools/gbmb18/GBMB.EXE res/map4.gbm`) for the user to hand-draw instead.
2. **User hand-drew map4 in GBMB**, then marked it up with a colored screenshot (green=player start, red=enemy spawns ["portales" in their terminology], purple=exit portal, black=doors). Door tile coordinates were given precisely (read off GBMB's own status bar); player/spawn/portal positions were read from the screenshot by rendering the actual saved `.gbm` tile data as a comparable image and cross-matching landmarks (same technique as the original map5 coordinate-extraction work) — precision matters for doors (must land exactly in a real wall gap or they gate nothing, learned from the v1 failure), not for spawns/player/portal (any floor tile is fine).
3. **Wired into `Rooms.c`, room index 3 (map4)**: 3 doors, 11 spawns, 1 portal, all with real BFS-verified reachability under different door-open combinations (same method as the map5 session). Spawn-lock scheme: only 2 spawns active at game start, each door's `unlock_spawn_indices` list grouped by what BFS showed it actually gates (or, when a door didn't physically gate anything, matched thematically to a nearby spawn cluster) — full reasoning is in the comment block above `room3_doors` in `Rooms.c`, don't duplicate it here, it changed shape several times as the user iterated (door shifted, spawns respaced/reordered/rolled back — current final coordinates are whatever's live in `Rooms.c` right now, always trust the file over this prose).
4. **Found and fixed 2 real bugs along the way**:
   - **Tile ID 5 rendering as the letter "A"**: `tiles.gbr` only defines 4 real tiles (0-3, confirmed via `Debug/res/tiles.gbr.c`'s `num_tiles`); the user had painted tile-ID-5 cells in GBMB as their own visual door-position markers, which — being out of the tileset's valid range — rendered as whatever leftover VRAM data happened to be there (the HUD font's 'A' glyph, by coincidence of VRAM layout). Not a code bug; fixed by patching those specific tiles back to floor (ID 4) directly in the `.gbm` binary once the door coordinates were captured and no longer needed the markers.
   - **Sprite "cutting" (partial/missing sprite rendering) that reacts to player position** — this is a **real Game Boy hardware limit** (max 10 sprites per scanline, 40 OAM entries per frame total; see `ZGB_extracted/ZGB/common/src/Sprite.c`'s `DrawSprite`/`next_oam_idx`), not a logic bug. This room keeps 3 doors + the portal permanently "alive" (`lim_x=255,lim_y=255`, so they work off-screen) on top of the player and however many enemies are active — when several of those share the same screen rows, the budget gets exceeded and the lowest-priority sprite's tiles get dropped, which is exactly why it correlated with the player's own position (moving into the crowded row tips it over the edge). **Mitigated (not eliminated — this is a hardware ceiling, not a bug with a real fix)** by spreading the bottom spawn cluster + portal across 3 different rows (14/15/16) instead of all on one row, so far fewer sprites ever compete for the same scanline. **Worth remembering for any future room**: don't cluster many spawns/portals/doors on the same row, especially in a room with multiple always-alive door sprites.
5. Debug scaffolding: `StateGame.c`'s `DEBUG_START_ROOM` is `3` (normal value `0`) — game boots straight into map4/room3 for testing, kept on purpose, per explicit request this session. Flag before shipping.

### Next session should start with

- User playtesting the current door/spawn/portal layout in BGB (last coordinates are live in `Rooms.c`, see the comment block above `room3_doors` for the full table).
- Nothing committed — decide with the user whether to commit once they're happy with the layout, and remember to flag `DEBUG_START_ROOM` before any real ship.

---

## Session addendum (2026-09-03) — map4 (room index 3) reworked into a real maze

User's complaint: map4's old layout (see git history, unchanged since `6b71b34`) had scattered short wall fragments that didn't form a real path — the door (232,24) and portal (280,24) sat in the same open, unwalled row, so the door never actually gated anything.

**Rework, same 40×18 map size:** generated a proper single-path maze (Python recursive-backtracker, `res/map4.gbm` binary tile data patched directly — same technique as the map5/mapboss work, see [[reference-ready-gamer-map-editing]]) — one unique corridor from the top-left player start to the bottom-right portal, no loops/shortcuts. One tile along that unique path was left as floor with the **Door sprite** sitting there instead of a wall — since doors block movement themselves (`SpritePlayer.c: CollidesWithClosedDoor`), that's a real chokepoint. Verified with a BFS reachability check (portal/spawns reachable with door open vs. closed) before wiring into `Rooms.c`.

New room3 layout (`Rooms.c`):
- Player start (1,1) tile → pixel (8,8).
- 1 door (10 coins), tile (28,9) → pixel (224,72).
- 4 spawns: 2 before the door (tiles (7,3)/(21,11)), 2 after (tiles (31,5)/(37,11)) — the 2 "after" spawns are unreachable until the door opens.
- Portal, tile (37,15) → pixel (296,120).

No spawn-locking (`initially_locked`/door unlock lists) used here — matches map4's original simplicity, only the path logic changed. Rebuilt (`make run BUILD_TYPE=Debug`) — compiled clean, BGB launched on the new ROM. **Not yet playtested by the user** — next step is playing it in BGB to confirm the maze feels right and the door genuinely blocks the portal path.

Nothing committed yet — `src/systems/Rooms.c` and `res/map4.gbm` are the only changes this session.

---

## Session addendum (2026-09-05) — room3 door/spawn regrouping, portal scanline-priority fix, and a real engine-wide sprite cap found

**This session's changes are now committed and pushed** (see commit at HEAD of `feature/map5-spawn-locks`) — this addendum documents what shipped and, importantly, one real bug that was found but deliberately NOT fixed yet (see "Not fixed" below, don't assume it's handled).

### Room3 (map4) layout changes, in order

1. **Regrouped door→spawn unlocks** per the user's explicit mapping: P1 unlocks S1/S6/S8, P2 unlocks S2/S3/S4 (was P3's group), P3 unlocks S9/S10/S11 (was P1's group). Coordinates re-specified individually across several iterations (S1→(4,4), S2→(37,4), S3→(35,4), S4→(33,4), S8→(4,15)) — all verified against the real `map4.gbm` tile data (parsed directly, same technique as prior sessions) to confirm none land on a wall tile.
2. **Simplified further, same session**: since spawn *count* doesn't affect spawn *rate* (see the spawn-rules explanation below — confirmed by reading `SpawnEnemies()`/`GetRandomSpawnPosition`), the user had S2/S3/S10/S11 removed entirely — one spawn per door group is enough. Final room3 spawn list: S1, S4, S5, S6, S7, S8, S9 (7 total, down from 11). Door unlock lists shrunk to match (`room3_door_p1_unlocks={0,3,5}`, `_p2_unlocks={1}`, `_p3_unlocks={6}`).
3. **Exit portal repositioned several times** during live playtesting (bugs found along the way, see below) — final position **(9,15)**, right next to S9.

### Real bug found and fixed: exit portal was losing the GB's 10-sprites-per-scanline race

User reported the exit portal specifically (not the spawn markers) visually "cutting" when sharing a scanline row with several always-alive spawn markers. Root cause, confirmed by reading `SetupRoomEntities` (`Rooms.c`): sprites are added in this order — `SpawnDoors` → `SpawnSpawnPoints` → `SpawnPortals` — and the GB hardware drops sprites past the 10-per-scanline cap **by OAM order** (lowest index wins). Since the portal was added *last*, it always had the lowest priority on any shared row and was the one dropped. **Fixed**: reordered to `SpawnDoors` → `SpawnPortals` → `SpawnSpawnPoints`, so the portal now outranks spawn markers for OAM slots on a shared scanline.

(`NextLevelPortal` and `SpawnPoint` do use different animation frames of the same `spawner` graphic — 4-8 vs 0-3 — confirmed they're visually distinguishable; that was a dead-end theory, not the actual bug.)

### Real bug found, NOT fixed — deliberately deferred, resume here

Separately, the user reported the **player's bullet** (`SpriteScrew`) going invisible/flickering near (10,15). Traced this to something bigger than per-scanline cutting:

- ZGB's sprite manager (`ZGB_extracted/ZGB/common/include/SpriteManager.h:8`, `N_SPRITE_MANAGER_SPRITES 20`) caps **total concurrent sprites at 20, project-wide** — not per-room, per-scanline. Doors, spawn markers, portal, player, enemies, and bullets all share this one pool.
- **Doors already free their slot correctly** — `HandleDoorInteraction` (`SpritePlayer.c:190-195`) calls `SpriteManagerRemove` right when a door opens. Not part of the problem.
- **Spawn markers and the portal never get removed**, ever — not when locked, not when superseded by another door's unlock group. They sit in the pool for the room's entire lifetime.
- Room3 right now: 3 doors (until opened) + 7 spawn markers + 1 portal + 1 player = up to 12 permanently-occupied slots, leaving as few as 8 for all enemies + bullets + pickups combined. Easy to exhaust with a few enemies alive.
- **Worse**: `SpriteManagerAdd` (`ZGB_extracted/ZGB/common/src/SpriteManager.c:102`, `sprite_idx = StackPop(sprite_manager_sprites_pool)`) never checks whether the pool is empty. `StackPop` (`include/Stack.h:18`, `#define StackPop(STACK) (*(STACK--))`) has zero bounds checking — if the pool is exhausted, this reads memory *before* the pool array and returns garbage as a sprite index. That's a plausible mechanism for a bullet silently failing to spawn (or, worse, corrupting whatever sprite happens to occupy that garbage index) instead of failing safely.
- This is **engine code** (`ZGB_extracted/`, gitignored — but the archive `ZGB.zip` it's unzipped from IS tracked in git, so a teammate re-unzipping it gets the same source).

**Discussed fix options with the user**: (a) patch `SpriteManagerAdd`/`StackPop` defensively so a full pool fails safely instead of reading garbage, (b) make spawn markers exist only while unlocked (create on unlock, destroy on lock/room-load-locked) to free up slots, (c) both. **User's call: do neither tonight** — leave the 20-sprite ceiling as a known, documented risk and revisit later. Don't silently "fix" this in a future session without flagging it first — it was an explicit deferral, not an oversight.

### Enemy spawn rules (explained this session, for reference)

- `level_spawns[level][]` / `level_lengths[level]` (`StateGame.c`) fix the **type** and **count** of enemies per wave — deterministic, not random.
- `spawn_timer` (`ENEMY_SPAWN_DELAY = 180` frames, ~3s) gates spawn **rate** — one enemy per tick, always, regardless of how many spawn points are unlocked.
- `GetRandomSpawnPosition` (`Rooms.c`) only affects **where** each enemy appears — picks randomly among currently-unlocked spawn indices. More unlocked spawns ≠ faster spawning, only more location variety. This is what justified deleting S2/S3/S10/S11 above.

### Debug scaffolding still in place

`StateGame.c`: `DEBUG_START_ROOM = 3` (normal value `0`) — boots straight into room3/map4 for testing. Flag before any real ship.

### For team review

The substantive logic change this session is entirely in **`src/systems/Rooms.c`** (door/spawn/portal tables for room3, plus the `SetupRoomEntities` reorder). `res/map4.gbm` only changed via earlier sessions' hand-drawing (binary map data, not meaningfully diff-reviewable). The 20-sprite engine cap finding above isn't a diff in this commit — it's a pre-existing latent bug in ZGB itself, worth a heads-up to anyone else building rooms with lots of always-alive sprites (doors/spawns/portals), since it can silently degrade instead of erroring.

## Session 2026-09-18: crash in map4 (room 3) level 14 — sprite pool guard (A) + unlocked-only spawn markers (B)

**Crash**: BGB "invalid opcode" at `ROM1:52CA` (bank 1 = music data), map4/room 3, Level 14, Coins 2, Lives 1. Root cause: ZGB's 20-sprite pool has no bounds check. Room 3 had 12 permanent sprites (player, 3 doors, 7 spawn markers, portal) + 6 enemies + bombs + 2 shots > 20. Map1 with same stats did not crash (5 permanent sprites).

**Fix (uncommitted, user confirmed no crash in that state)**:
- A: new `include/SpriteBudget.h` — `SafeSpriteAdd/AddEx` (NULL at 20) and `SafeSpriteAddLow` (NULL at 18, keeps 2 slots for player shots). Used for enemies (`StateGame.c`, retry after `SPAWN_RETRY_DELAY`=30 frames when full), periodic bombs (`BomberVirus.c`), death bombs (`SpriteScrew.c`, `ElectricProjectile.c`), player shots with NULL-checks (`SpritePlayer.c`, `BossRunPlayer.c`, `BossFightPlayer.c`).
- B: `Rooms.c` `SyncSpawnMarkers` — spawn marker sprites exist only for unlocked spawns; called on room load, door open (`ApplyDoorSpawnUnlocks`), wave clear (`EnsureRoomSpawnPoints`). Room 3 permanent sprites 12 -> 7. Markers are visual only (enemy positions come from the table).
- Declined by user: C (portal on demand), D (max 1 Bomber per wave). User said do not touch `Boss.c`.

**Static worst-case analysis** (fixed + enemies + 1 extra bomb per Bomber + 2 shots): levels 1-16 max 18 in every room; 17-19 up to 19-20 (guard may delay spawns/skip bombs); level 20 21-22 in rooms 1-4 (guard must engage).

**Still open**: `StateBossRun.c:96,115` raw `SpriteManagerAdd` (enemies removed only when off left of camera — can pile up if not killed); `Boss.c` adds/derefs unguarded. Not playtested: levels 17-20 in rooms 1/4, full boss run, rooms 0/1/2/4 re-verify.

**Working tree**: `StateGame.c` DEBUG values (`DEBUG_START_ROOM=3`, `DEBUG_START_LEVEL=14`, `DEBUG_START_COINS=2`, `DEBUG_STARTING_LIVES=1`) must be reverted (3 -> 0, 14 -> 1, 2 -> 0, 1 -> `STARTING_LIVES`) before committing. Ask before committing.

## Session 2026-09-18 (end of day): Release ROM built for physical cartridge — hardware risk review pending 2026-09-19

- Guarded `StateBossRun.c` adds (`SafeSpriteAdd` for bullets, `SafeSpriteAddLow` for enemies); `StateGame.c` DEBUG macros back to release values (room 0, level 1, coins 0, `STARTING_LIVES`).
- Clean Release build: `bin/READY_GAMER.gb`, 128 KB, MBC1 (type 01), no RAM, header + global checksums valid. Launched in BGB. **Not committed.**
- **Risks to review next session** (full detail in the memory file `project_ready_gamer_hardware_release.md`):
  1. `next_round_timer` is `UINT8` but `NEXT_ROUND_TIMER` = 300 -> overflows to 44 (warning 158): ~0.7 s between waves, pre-existing. Not changed — balance decision.
  2. Banks 2 and 3 have 21 and 5 bytes free — freeze the ROM after testing.
  3. Header title empty (cosmetic).
  4. Real DMG boots with random RAM — test in SameBoy/mGBA; uninitialized locals in old code not audited.
  5. Release not fully played: rooms 0/1/2/4, levels 17-20, boss run, boss fight (`Boss.c` intentionally untouched per user).
- Decisions: C (portal on demand) and D (1 Bomber per wave) declined; do not touch `Boss.c`.

## Session 2026-09-21: hardware-ready Release build (MBC5 cartridge) — uncommitted

Built `bin/READY_GAMER.gb` (Release, 128 KB, SHA-256 `e45bfbc9...` (after the HUD fix below); backup copy kept outside the repo). Changes, all uncommitted on top of the 09-18 sprite-pool work:
- **Header** (`src/Makefile`, `BINFLAGS += -yt 0x19 -yn "READY GAMER"` after the engine include): type MBC5 (0x19), title `READY GAMER`, header checksum 0x37 and global A915 verified. Game bank switching only writes 0x2000 (8-bit bank), which MBC5 and MBC1 treat the same; banks used are 1-6, never 0. Changing the Makefile forces a full `make clean` (wipes shared `bin/`, so the Debug ROM is gone until rebuilt).
- **`next_round_timer`** (`StateGame.c`) UINT8 -> UINT16: `NEXT_ROUND_TIMER 300` used to overflow to 44 (~0.7 s between waves), now the intended 300 (5 s). Balance change: set `NEXT_ROUND_TIMER` to another value if 5 s feels slow. Warning 158 gone.
- **Wave RAM** (`SoundEffects.c` `InitWaveRam() BANKED`, called from `StateMenu.c START()` before `PlayMusic`): channel-3 SFX only turn the wave channel on and play whatever wave RAM holds, which is random on a real DMG. Loads a triangle wave (NR30 off during the copy, restored after; copy runs inside `CRITICAL` so the music timer ISR cannot interleave).
- **Soft reset** (`include/SoftReset.h`, `CHECK_SOFT_RESET()` at the top of `UPDATE()` in `StateGame`, `StateBossRun`, `StateBossFight`): A+B+START+SELECT held -> GBDK `reset()`. A cartridge has no reset button.
- Free bytes after build: bank 0 = 248, bank 2 = 16, bank 3 = 8, banks 4-6 mostly empty. Freeze the ROM after the last test.

**Not yet verified in a running emulator** (nothing was played): test in Emulicious (`ZGB_extracted/ZGB/env/emulicious`) with random uninitialized memory and the exceptions for inaccessible VRAM/OAM/palette, LCD off outside vblank, OAM bug, wave RAM corruption, bus conflict with OAM DMA, non-standard MBC address. The BGB "inaccessible VRAM" stop when entering map5 (2026-09 note above) is NOT proven harmless on hardware: a write during mode 3 is dropped. `SetTile` waits for STAT correctly; the flagged routine is unidentified.

**Second review (same day), findings:** clean rebuild is reproducible (same hash twice). No writes to ROM/MBC addresses except `0x2000`; the only LCD-off path is GBDK's `display_off` (waits for vblank) and `wait_vbl_done` returns at once when the LCD is off, so the room-transition sequence cannot hang. Boss fight worst case is about 13 of 20 sprites (shots limited by 100-frame cooldown vs 120-frame life, max 2 bosses), so unguarded `Boss.c` adds stay safe without touching it. **Corrected diagnosis of the BGB "inaccessible VRAM" stop:** `set_vram_byte`/`set_bkg_tile_xy` (GBDK) and ZGB `SetTile` check STAT bit 1 and then write, but an interrupt (music timer, LYC/STAT, VBL) landing between the check and the write can push the write into mode 3, where hardware drops it. Effect: rare single wrong background tile until that cell is redrawn; game logic uses the room tile table, so collisions are unaffected. Not fixed (would need a `di/ei` in the engine's `SetTile`, i.e. patching ZGB). Frame budget (room 3 with ~18 sprites) still unmeasured.

**CRITICAL FIX (same day): the Release ROM had no HUD and no sprites.** Found because the user saw an empty room 0 in BGB and Emulicious. ZGB's `Print.h` compiles `INIT_CONSOLE`/`DPrintf`/`DPRINT_POS` to nothing when `NDEBUG` is defined (Release defines it). This game draws its HUD (Level/Coins/Lives) with them, and `INIT_CONSOLE` is also what calls `SetWindowY`, which makes ZGB's LYC interrupt (`LCD_isr`) show sprites. In Release the menu's `WY=144`/`LYC=160` stayed, the interrupt never fired and sprites stayed hidden (the 09-15 "room 0 shows no sprites" report and the 09-18 hardware Release had the same bug; only Debug builds had been played). Fix: `CFLAGS += -UNDEBUG` in `src/Makefile`. Now WY=128, LYC toggles, HUD and sprites show.

**Headless verification (PyBoy, `pip install pyboy`; scripts are in the session scratchpad, not the repo):** boot -> menu -> Start -> room 0 shows HUD + 12 sprites; A+B+Start+Select returns to the menu; 8 of 8 boots with random RAM/VRAM/OAM/HRAM (ROM stub patched in) reach the menu and a full room 0; random-input play of 25-40 s in rooms 0-4 (level 20 in rooms 1-4) never crashed or froze and peaked at 15 of 20 sprites; boss run and boss fight run 60 s; Win and GameOver screens draw and return to the menu. Not covered: sound, real DMG timing/LCD, real cartridge.

Build note: if BGB has `bin/` open, `make clean` fails with "Permission denied" (Makefile changes trigger an automatic clean). Workaround used: delete `Release/`, `mkdir Release`, `touch Release/Makefile.uptodate`, then `make build_gb BUILD_TYPE=Release`.

**Room 3 (map4) door P1 moved one tile right** (user request): `Rooms.c` `room3_doors[0]` tile x 2 -> 3, i.e. it now plugs the 1-tile-wide gap in the wall column x=3 (rows 10-11) instead of sitting in the 2-wide left corridor. Checked headless: the door still blocks the horizontal corridor at rows 10-11. Side effect: the left corridor (x=1-2) no longer needs P1, so the dead-end pocket at the bottom-left (S7 area) is reachable without opening it. New ROM SHA-256 starts `018e40ba`.
