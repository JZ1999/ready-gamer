# Ready Gamer — agent context

Game Boy (DMG) original built with **ZGB** for the **Ready Games** business. Playable in-browser via the arcade (`readygames-flash`) using EmulatorJS; catalog/API lives in Django (`buy-games`).

## Business / product

- Brand: **Ready Games** (Costa Rica arcade + store). Production arcade/API: `https://readygamescr.com`, store: `https://store.readygamescr.com`.
- In-game currency HUD: **Ready Coins** (`ready_coins` in `StateGame.c`).
- Arcade listing slug: **`ready-gamer`**. Upload debug/release `.gb` + thumbnail in Django admin as a Flash Game with `embed_type=gameboy`.
- Thumbnail source for marketing: main-menu screenshot → JPG (e.g. `ready_gamer_menu.jpg` in project root for manual upload). Do not treat it as a required build artifact.

## Sibling repos (WSL)

| Role | Path | Remote |
|------|------|--------|
| Arcade frontend | `/home/joseph/readygames-flash` | `JZ1999/readygames-flash` |
| Django API / admin / media | `/home/joseph/buy-games` | `JZ1999/buy-games` |
| This ROM project | `C:\Users\josep\Documents\Projects\ready gamer` | `JZ1999/ready-gamer` |
| Engine (local) | `C:\ZGB` or unzipped `ZGB/` via `ZGB_PATH` | — |

Local arcade play: `http://localhost:3002/game/ready-gamer` (API `http://localhost:8000`).

## Engine & build

- Framework: **ZGB** + GBDK. Set `ZGB_PATH` to the ZGB `common` folder, then `build.bat` / `build_Debug.bat`.
- `clean.bat` after changes under `include/` or ZGB itself.
- Outputs (gitignored): `bin/`, `Debug/`, `*.gb`. Typical debug ROM name: **`READY_GAMER_Debug.gb`** (also sometimes copied to project root).
- Workspace also mounts `c:\ZGB` for engine browsing.

## Layout

```
include/          Public headers (ZGBMain.h, Rooms.h, BossRun.h, StateGame.h, SoundEffects.h, SpriteData.h)
src/states/       StateMenu, StateGame, StateGameOver, StateBossRun, StateWin
src/sprites/      Player, enemies, pickups, doors, portal, bomb, projectiles; boss run: BossRunPlayer, CameraDriver, BossBullet
src/systems/      Rooms.c, SoundEffects.c
src/assets/       Generated/hand-authored tile+map C data (GBTD/GBMB exports)
res/              .gbm maps (incl. **mapboss**), .gbr sprites (GBTD/GBMB sources)
```

States in `include/ZGBMain.h`: `StateMenu` → `StateGame` → `StateGameOver`; **`StateBossRun`** (finale); **`StateWin`** (raffle code UI).

## Multi-room system

- Table-driven rooms in `src/systems/Rooms.c` (`MAX_ROOMS` / `room_count` = **5**).
- Maps: `map` (room 0), `map2`…`map5` under `res/`.
- Each `RoomDef`: map, player start, doors, spawn points, portals, optional electricity / coins placements.
- Wave difficulty is indexed by **`current_room`** (room 0 = level 1). See `StartRoomEnemyWave()` in `StateGame.c`.
- Cross-bank scroll/collision helpers live in `ZGBMain.c` (call from home bank when needed).
- Adding a room: new `res/mapN.gbm` → `IMPORT_MAP` → placement arrays → append `RoomDef` → bump `room_count` / `MAX_ROOMS`.

### Pickups by room (current)

- Electricity: room 1 (level 2) only.
- Coins: rooms 2 and 4 (levels 3 and 5).

### map4 note

Exit alcove / portal path was previously blocked by pillars; map geometry was patched so door/portal near the top-right remain reachable.

## Combat & enemies

Spawn table in `StateGame.c` (`level_spawns` / `level_lengths`, **20 waves**):

| Levels | Introduces / feel |
|--------|-------------------|
| 1–3 | Basics (+ Speed) |
| 4–5 | BomberVirus |
| 6–7 | ChargeVirus |
| 8–9 | TankVirus |
| 10–20 | Full mix; pack size and elites ramp up |

After wave 20, `current_level` wraps to 1. Clearing the portal in the **last room** (`map5`) enters **`StateBossRun`** (auto-scroll corridor on `mapboss`). Reaching the right edge of that map enters **`StateWin`** (raffle code UI, e.g. `RGGBA26`).

## Boss run (finale — WIP content)

Thematically tied to the **Ready Games** video-game store: implied boss (no body sprite); player runs long horizontal **mapboss** while dodging bullets and surviving camera crush.

| System | Location |
|--------|----------|
| State + bullets | `src/states/StateBossRun.c` |
| Auto-scroll | `src/sprites/CameraDriver.c` (`scroll_target`, ~30 px/s) |
| Player + i-frame walls | `src/sprites/BossRunPlayer.c` |
| Boss attack | `src/sprites/BossBullet.c` (bomb gfx) |
| Collision | `src/ZGBMain.c` — `BossRunTranslateSprite` / `BossRunTranslateSpritePhasing` |
| Map | `res/mapboss.gbm` — **240×18** tiles; rows 0 and 17 = ceiling/floor |

**Wall rules (boss only — rooms 0–4 unchanged):** normal move uses full collision + diagonal slide; while invincible, phase through **interior** walls in any direction but **not** ceiling/floor rows; when i-frames end inside an interior wall, auto-push **right** until clear.

**Still TODO:** map geometry / store-aisle pacing (see `SESSION_NOTES.md`). **Local debug cheats** (menu→boss, 100 coins, dmg 10) — revert before release.

Sprites (`ZGBMain.h` ↔ gfx basename):

- `BasicVirus`, `SpeedVirus`, `TankVirus`, `BomberVirus` (drops `Bomb` on death), `ChargeVirus` (wind-up then dash).
- Player: `SpritePlayer`; shots: `SpriteScrew`, `ElectricProjectile` (after electricity pickup).
- Transitions: `Door`, `NextLevelPortal`, `SpawnPoint`.

### ChargeVirus art vs mechanics (Windows)

- Mechanics: `src/sprites/ChargeVirus.c` — **keep behavior here**.
- Art: `src/assets/chargeVirusGfx.c` — separate file so Windows does not collide `chargeVirus.c` vs `ChargeVirus.c`.
- `ZGBMain.h` maps `ChargeVirus` sprite to gfx name `chargeVirus`.

## Critical gotchas

1. **Banked sound**: `SoundEffects` APIs must be `BANKED` when called from other banks (e.g. StateGame bank 2 → SoundEffects bank 3). Missing `BANKED` caused white-screen crashes.
2. **Off-screen cull**: set `THIS->lim_x = 255` / `THIS->lim_y = 255` on enemies, bomb, doors, portals, spawners, pickups so they are not destroyed when scrolling.
3. **Game Over**: use a real tileset (`InitScrollWithTiles`); hide the HUD window; prefer font text over mismatched MenuTileset center art.
4. **Electricity pickup**: sets `pending_electric_pickup`; StateGame grants the electric attack on the next safe tick (avoid mutating player mid-collision).

## Deploy path to arcade

1. Build `READY_GAMER_Debug.gb` (or release).
2. In `buy-games` admin: Flash Game `ready-gamer`, embed Game Boy, upload ROM (+ JPG thumb).
3. Arcade (`readygames-flash`) loads `embed_type === 'gameboy'` via EmulatorJS; `play_url` must be the Django `/api/flash-games/<slug>/rom/` proxy (raw private S3 signed URLs fail CORS from Vercel).

See also: `readygames-flash/CONTEXT.md` and `buy-games` `flash_games` app (`rom_file`, migration `0003_flashgame_gameboy_rom`).
