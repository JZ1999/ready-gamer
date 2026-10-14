# Fast headless e2e (Linux / WSL / cloud)

Same scenarios and same in-ROM autopilot as `e2e.bat`, but builds with Linux
GBDK and runs in a patched [Peanut-GB](https://github.com/deltabeard/Peanut-GB)
(MIT) instead of BGB. A full scenario takes seconds instead of minutes.

```bash
tools/e2e/linux/setup.sh            # once
tools/e2e/linux/e2e.sh electric     # build + run one scenario
tools/e2e/linux/e2e.sh full_clear 7200  # max game seconds
python3 tools/e2e/linux/sheet.py tools/e2e/artifacts/linux/full_clear   # contact sheet PNG
```

Output in `tools/e2e/artifacts/linux/<scenario>/`:

- `result.txt`: `verdict=PASS|FAIL|TIMEOUT|NONE`, frames
- `telemetry.csv`: RAM symbols every 30 frames (room, level, coins, lives,
  wave counters, bot keys, bot position, bot branch `e2e_dbg`). Add more with
  `EXTRA_SYMS="_e2e_ex:2 _e2e_ey:2"`; any global in `Debug/rom.noi` works.
- `frames/*.pgm` every N frames (3rd arg), `final.pgm`

How it works: `rgrun` treats `ld b,b` (`EMU_BREAKPOINT`) as the end of the run
and reads the verdict frame the ROM paints (all light = PASS, all dark = FAIL),
exactly like the BGB runner.

Notes:
- Resource `.c` files are reused from the Windows `Debug/res` build (the
  asset converters are Windows-only). Rebuild on Windows after editing `res/`.
- Peanut-GB timing differs slightly from BGB; confirm important results with
  `e2e.bat` (BGB) before trusting them for release.
