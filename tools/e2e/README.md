# Ready Gamer e2e (BGB)

In-ROM autopilot + headless BGB. Pure TAS demos are brittle here because enemy
spawns use `rand()` — scenarios drive the state machine / pickups instead.

## Scenarios

| Name | Id | Asserts |
|------|----|---------|
| `die` | 1 | Walk into enemies until real GameOver |
| `room1_die` | 2 | Earn coins, buy door, portal, die in room 1 |
| `full_clear` | 3 | Whole game from room 0 with 0 coins: farm coins, buy doors, pick up every power-up (electricity, coin pickups, fire-rate chip), all 5 rooms → boss run → boss fight → Win |
| `electric` | 4 | Walk to room0 electricity pickup (real collision) |
| `coins` | 5 | Earn coins by killing enemies |

No free coins, enemy wipes, wall-phasing, or skip-to-win. Movement uses
`SafeTranslateSprite`; shots use real projectiles; doors cost earned coins.
Default suite: `die,room1_die,electric,coins` plus `full_clear` on the Linux runner.

`full_clear` runs up to 2 game hours: use the fast Linux runner
(`tools/e2e/linux/README.md`); don't `-Capture` it in BGB (2 GB AVI cap).
There are no single-level tests: every level is played in order, carrying
the coins and power-ups earned on the way, like a real player.

## Run

```bat
e2e.bat
e2e.bat -Scenarios die,electric,coins
e2e.bat -Capture
```

`-Capture` records an AVI, extracts PNG frames for in-browser playback in the
HTML report, and re-runs once for a final PNG (RecordAVI blanks `-screenonexit`).

Requires `ZGB_PATH` pointing at ZGB `common`.

## Review artifacts

After a run, open:

`tools/e2e/artifacts/index.html`

Per scenario folder:

- `final.png` — last frame (`bgb -screenonexit`; more reliable with `-Capture`)
- `*.avi` — only with `-Capture` (gameplay video)
- copy of the ROM used for that run

Headless `-hf` is for fast CI pass/fail. Use `-Capture` when you want to
watch what happened (PNG + AVI).

## How it works

1. Writes `include/E2E_scenario.h` with the scenario id
2. `make build_gb BUILD_TYPE=Debug`
3. Copies the ROM to `%TEMP%\rge2e\<scenario>\` (BGB breaks on paths with spaces)
4. `bgb -hf -autoexit -screenonexit …` (or visible + AVI with `-Capture`)
5. ROM hits `EMU_BREAKPOINT` on pass → BGB exits 0
6. Artifacts copied back to `tools/e2e/artifacts/`
7. Timeout / no breakpoint → fail
8. Restores `E2E_SCENARIO` to `0`
