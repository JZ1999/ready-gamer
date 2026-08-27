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
