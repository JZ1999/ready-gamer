# Session notes (local — read first each session)

Last updated: 2026-08-27. Branch: `feature/map5-spawn-locks`.

## Done this session — boss run physics

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
