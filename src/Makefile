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