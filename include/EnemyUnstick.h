#ifndef ENEMY_UNSTICK_H
#define ENEMY_UNSTICK_H

#include "Sprite.h"

// Stuck recovery for chasing enemies (Tank, Speed, Bomber, Charge; Basic has
// its own older version). Greedy chase + EnemyMoveWithWallAvoidance can
// jitter 1px back and forth against a wall forever without ever reporting
// "blocked", so this detects lack of progress instead: if an enemy that is
// not next to the player hasn't moved far in a while, it walks sideways
// (perpendicular to where it was trying to go) for a few steps, then resumes
// chasing. BANKED: called from sprite banks.

// Clear this sprite's tracking (call in the enemy's START: pool slots are
// reused by new sprites).
void EnemyUnstickReset(Sprite* enemy) BANKED;

// Call once per movement tick, after computing the chase step (*dx, *dy) and
// before moving. While escaping, it overwrites *dx/*dy with the escape step.
void EnemyUnstickSteer(Sprite* enemy, INT8* dx, INT8* dy) BANKED;

#endif
