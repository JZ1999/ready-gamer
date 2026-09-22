PROJECT_NAME = READY_GAMER

all: build_gb

# Number of banks (must be a power of 2): A (Automatic), 2, 4, 8, 16, 32...
N_BANKS = A

# Music player: HUGETRACKER(default) or GBT_PLAYER
MUSIC_PLAYER = HUGETRACKER
#MUSIC_PLAYER = GBT_PLAYER

# Default hardware sprites size: SPRITES_8x16(default) or SPRITES_8x8
DEFAULT_SPRITES_SIZE = SPRITES_8x16

include $(ZGB_PATH)/src/MakefileCommon

# Physical cartridge header: MBC5 (0x19, flash carts) + title. These come after
# the engine's own "-yt 1" so makebin takes the last -yt. Title is max 15 chars.
BINFLAGS += -yt 0x19 -yn "READY GAMER"

# The HUD (Level/Coins/Lives) is drawn with INIT_CONSOLE/DPrintf, which ZGB's
# Print.h compiles to NOTHING when NDEBUG is defined (Release does). Without it
# the window is never set up, the LYC interrupt never runs and every sprite
# stays hidden. Keep NDEBUG off so Release matches the tested Debug behaviour.
CFLAGS += -UNDEBUG

# Keep map/map2 in ROM bank 2 so room0/room1 scroll_map stays stable.
# map3 lives in bank 3 — bank 2 is already near full with maps + game code.
$(OBJDIR_RES)/map.gbm.c: ../res/map.gbm
	@$(GBM2C) $< $(OBJDIR_RES)
	@powershell -NoProfile -Command "(Get-Content -Raw '$(OBJDIR_RES)/map.gbm.c') -replace '#pragma bank 255','#pragma bank 2' -replace 'const void __at\(255\) __bank_map','const void __at(2) __bank_map' | Set-Content -NoNewline '$(OBJDIR_RES)/map.gbm.c'"

$(OBJDIR_RES)/map2.gbm.c: ../res/map2.gbm
	@$(GBM2C) $< $(OBJDIR_RES)
	@powershell -NoProfile -Command "(Get-Content -Raw '$(OBJDIR_RES)/map2.gbm.c') -replace '#pragma bank 255','#pragma bank 2' -replace 'const void __at\(255\) __bank_map2','const void __at(2) __bank_map2' | Set-Content -NoNewline '$(OBJDIR_RES)/map2.gbm.c'"

$(OBJDIR_RES)/map3.gbm.c: ../res/map3.gbm
	@$(GBM2C) $< $(OBJDIR_RES)
	@powershell -NoProfile -Command "(Get-Content -Raw '$(OBJDIR_RES)/map3.gbm.c') -replace '#pragma bank 255','#pragma bank 3' -replace 'const void __at\(255\) __bank_map3','const void __at(3) __bank_map3' | Set-Content -NoNewline '$(OBJDIR_RES)/map3.gbm.c'"

$(OBJDIR_RES)/map4.gbm.c: ../res/map4.gbm
	@$(GBM2C) $< $(OBJDIR_RES)
	@powershell -NoProfile -Command "(Get-Content -Raw '$(OBJDIR_RES)/map4.gbm.c') -replace '#pragma bank 255','#pragma bank 3' -replace 'const void __at\(255\) __bank_map4','const void __at(3) __bank_map4' | Set-Content -NoNewline '$(OBJDIR_RES)/map4.gbm.c'"

$(OBJDIR_RES)/map5.gbm.c: ../res/map5.gbm
	@$(GBM2C) $< $(OBJDIR_RES)
	@powershell -NoProfile -Command "(Get-Content -Raw '$(OBJDIR_RES)/map5.gbm.c') -replace '#pragma bank 255','#pragma bank 4' -replace 'const void __at\(255\) __bank_map5','const void __at(4) __bank_map5' | Set-Content -NoNewline '$(OBJDIR_RES)/map5.gbm.c'"

# Boss-run auto-scroll level (StateBossRun) — bigger than any room map, gets its own bank.
$(OBJDIR_RES)/mapboss.gbm.c: ../res/mapboss.gbm
	@$(GBM2C) $< $(OBJDIR_RES)
	@powershell -NoProfile -Command "(Get-Content -Raw '$(OBJDIR_RES)/mapboss.gbm.c') -replace '#pragma bank 255','#pragma bank 5' -replace 'const void __at\(255\) __bank_mapboss','const void __at(5) __bank_mapboss' | Set-Content -NoNewline '$(OBJDIR_RES)/mapboss.gbm.c'"

# Boss-fight static arena (StateBossFight), after the boss-run corridor.
$(OBJDIR_RES)/mapbossarena.gbm.c: ../res/mapbossarena.gbm
	@$(GBM2C) $< $(OBJDIR_RES)
	@powershell -NoProfile -Command "(Get-Content -Raw '$(OBJDIR_RES)/mapbossarena.gbm.c') -replace '#pragma bank 255','#pragma bank 6' -replace 'const void __at\(255\) __bank_mapbossarena','const void __at(6) __bank_mapbossarena' | Set-Content -NoNewline '$(OBJDIR_RES)/mapbossarena.gbm.c'"