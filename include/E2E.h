#ifndef E2E_H
#define E2E_H

#include <gbdk/platform.h>
#include "E2E_scenario.h"
#include "Keys.h"

#ifndef E2E_SCENARIO
#define E2E_SCENARIO 0
#endif

#define E2E_ENABLED (E2E_SCENARIO != 0)
#ifndef E2E_SEED
#define E2E_SEED 0 /* extra title-screen wait (x7 frames): a different game per seed */
#endif

/* The autopilot only chooses buttons: it ORs its pad state into ZGB's
 * `keys` at the top of each player sprite's UPDATE (and the menu), so all
 * game rules run unchanged. No-op in normal builds. */
#if E2E_ENABLED
extern UINT8 e2e_keys;
#define E2E_INJECT_KEYS() (keys |= e2e_keys)
#else
#define E2E_INJECT_KEYS()
#endif

#define E2E_SCENARIO_DIE           1 /* Die in room 0 → GameOver */
#define E2E_SCENARIO_ROOM1_DIE     2 /* Clear room 0, die in room 1 → GameOver */
#define E2E_SCENARIO_FULL_CLEAR    3 /* Whole game from room 0: farm coins, every power-up, all 5 rooms, boss run + fight → StateWin */
#define E2E_SCENARIO_ELECTRIC      4 /* Pick up electricity → electric attack on */
#define E2E_SCENARIO_COINS         5 /* Pick up coins → ready_coins increases */
/* Scenarios that use the whole-map route planner (RT_*, E2EPlan.c). */
#define E2E_USES_PLANNER (E2E_SCENARIO == E2E_SCENARIO_FULL_CLEAR)

void E2E_OnMenuUpdate(void) BANKED;
void E2E_OnGameUpdate(void) BANKED;
void E2E_OnBossUpdate(void) BANKED;
void E2E_OnBossFightUpdate(void) BANKED;
void E2E_OnGameOverStart(void) BANKED;
void E2E_OnGameOverUpdate(void) BANKED;
void E2E_OnWinStart(void) BANKED;
void E2E_OnWinUpdate(void) BANKED;

#endif
