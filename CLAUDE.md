# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Business/product context (Ready Games arcade, deploy path to the web arcade, sibling repos) lives in `CONTEXT.md` — read that first for anything non-technical. This file covers building and code architecture only.

Current work-in-progress status, uncommitted changes, and next steps live in `SESSION_NOTES.md` (not committed) — read that first each session to know where things stand.

## Build

- Requires the ZGB engine present locally with `ZGB_PATH` pointing at its `common` folder. `ZGB.zip` in the repo root is the engine archive — unzip it once (e.g. to `ZGB_extracted/`, gitignored) and set `ZGB_PATH=<path>/ZGB/common`.
- `build.bat` — release ROM. `build_Debug.bat` / `build_Color.bat` / `build_DebugColor.bat` — other variants. All of them just `cd src && make run BUILD_TYPE=<Release|Debug|ReleaseColor|DebugColor>` (make binary lives at `%ZGB_PATH%\..\env\make-3.81-bin\bin\make`). **`make run` also launches the BGB emulator** on the freshly built ROM — kill that process if building non-interactively.
- `clean.bat` — run after editing anything under `include/` or the ZGB engine itself; a plain rebuild doesn't pick those up.
- Output (gitignored): `bin/READY_GAMER_Debug.gb` (or matching variant name) and `Debug/rom.gb`.
- **Never build the shipping ROM with `NDEBUG` on**: ZGB's `Print.h` turns `INIT_CONSOLE`/`DPrintf` into no-ops under `NDEBUG`, which removes the HUD and leaves every sprite hidden. `src/Makefile` adds `-UNDEBUG` for that reason.
- No test suite, no linter — this is a C ROM built with SDCC via GBDK/ZGB.

## Architecture

### State machine
ZGB generates dispatch from macro tables in `include/ZGBMain.h`: the `STATES` list (`StateMenu` → `StateGame` → `StateGameOver`, plus `StateBossRun` and `StateWin`) and the `SPRITES` list (maps a sprite type name used in code to its gfx basename, e.g. `_SPRITE_DMG(ChargeVirus, chargeVirus)`). Each state/sprite is a `.c` file in `src/states/` or `src/sprites/` implementing `START()` / `UPDATE()` (sprites also `DESTROY()`) — there is no explicit registration in the `.c` files themselves, only the macro lists in `ZGBMain.h`.

### Rooms are table-driven, not per-level code
`src/systems/Rooms.c` holds a static `rooms[MAX_ROOMS]` array of `RoomDef` (map + bank, player start, doors, spawn points, portals, optional electricity/coin pickups). `StateGame.c` and `ZGBMain.c` never hardcode room content — they go through `GetCurrentRoom()` / `SpawnRoomEntities()`. To add a room: create `res/mapN.gbm`, add `IMPORT_MAP(mapN)` in both `Rooms.c` and `ZGBMain.c`, add placement arrays, append a `RoomDef`, and bump `room_count` / `MAX_ROOMS` (`include/Rooms.h`).

### Custom tile collision bypasses ZGB's map API
ZGB's stock scroll/collision helpers assume a single map bank; this project needs five (rooms 0-4 pinned across ROM banks 2-4 by a post-`GBM2C` bank-rewrite step in `src/Makefile`, so `scroll_map` doesn't drift between builds). `ZGBMain.c` reads tiles directly out of the active room's bank (`GetRoomTileFromTable`) and implements its own edge/partial-brick collision (`CheckEdgeMapCollision`, `SafeTranslateSprite`, `EnemyMoveWithWallAvoidance`) instead of relying on ZGB's built-in collision. New movement code in **room states** should go through `SafeTranslateSprite` / `EnemyMoveWithWallAvoidance`. **Boss run** uses separate `BossRunTranslateSprite*` helpers on `mapboss` only (see below).

### Wave/level system
`StateGame.c` tracks two counters that both drive difficulty: `current_room` (which of the 5 rooms) and `current_level` (1-20, wraps to 1), which indexes `level_spawns[20][10]` / `level_lengths[20]` for enemy mix and count per wave. `current_level` increments on wave-clear (`CheckForNextLevel`) **and** again on room transition (`UPDATE()`, `pending_room_transition` branch) — the two compound rather than being independent.

### Sprite conventions (apply to every new enemy/pickup/interactable)
- Off-screen-capable sprites (doors, spawners, portals, pickups, enemies) must set `THIS->lim_x = 255; THIS->lim_y = 255;` in `START()`, or ZGB culls them once the camera scrolls past its default 32px limit.
- Any function called across ROM banks (e.g. everything in `SoundEffects.c`, called from state/sprite banks) must be declared `BANKED` in its header — a missing `BANKED` compiles cleanly but crashes to a white screen at runtime.
- Per-sprite state lives in `THIS->custom_data[]`; indices are named per sprite family in `include/SpriteData.h` (`CD_ENEMY_HEALTH`, `CD_DOOR_STATE`, `CD_PLAYER_ELECTRIC`, etc.) — reuse those names rather than raw indices.
- Deferred-mutation pattern: anything that must not happen mid-collision-callback (electricity pickup granting the attack, room transition) sets a `pending_*` flag (`pending_electric_pickup`, `pending_room_transition`) that `StateGame.c`'s `UPDATE()` resolves on the next safe tick.

States registered in `include/ZGBMain.h`: `StateMenu` → `StateGame` → `StateGameOver`; **`StateBossRun`** (auto-scroll finale); **`StateWin`** (raffle screen after boss run).

### Boss run (`StateBossRun` / `mapboss`)

Separate from the five-room crawler: no `RoomDef`, doors, or spawns. Entered from the **map5 portal** (`StateGame.c` → `SetState(StateBossRun)`). Win when auto-scroll reaches the right edge of **mapboss** → `StateWin`.

| Piece | File | Notes |
|-------|------|--------|
| State | `src/states/StateBossRun.c` | Bullet timer, forces `scroll_y = 0`, win check |
| Auto-scroll | `src/sprites/CameraDriver.c` | Invisible `scroll_target`; +1px every 2 frames |
| Player | `src/sprites/BossRunPlayer.c` | Fork of `SpritePlayer`; `BossRunTakeDamage` |
| Attack | `src/sprites/BossBullet.c` | Spawns off right edge; reuses `bomb` gfx |
| Collision | `src/ZGBMain.c` | `BossRunTileBlocked`, `BossRunTranslateSprite`, **`BossRunTranslateSpritePhasing`** |
| Map | `res/mapboss.gbm` | 240×18 tiles; row 0 + row 17 = ceiling/floor walls |

**Invincibility wall rules (boss player only):**

- Normal move: full collision; diagonal slides per axis; edge-sweep collision.
- While invincible: phase through **interior** solids (any direction); **top/bottom map rows always solid**.
- When i-frames end inside an interior wall: auto **push right** until clear (`CD_PENDING_WALL_PUSH` in boss player — aliases unused `CD_PLAYER_ELECTRIC` index).
- Main-game rooms use `SafeTranslateSprite` — **not affected** by boss collision changes.

Boss is **implied** (no boss body sprite). Map geometry still WIP — long open corridor; see `SESSION_NOTES.md` for design ideas.

### Layout
```
include/          Public headers (ZGBMain.h, Rooms.h, BossRun.h, StateGame.h, SoundEffects.h, SpriteData.h)
src/states/       StateMenu, StateGame, StateGameOver, StateBossRun, StateWin
src/sprites/      Player, BossRunPlayer, enemies, pickups, doors, portal, CameraDriver, BossBullet, bomb, projectiles
src/systems/      Rooms.c (room table), SoundEffects.c (banked PSG sound effects)
src/assets/       Generated/hand-authored tile+map C data (GBTD/GBMB exports)
res/              .gbm maps, .gbr sprites — source files for the above, edited in GBTD/GBMB
```
