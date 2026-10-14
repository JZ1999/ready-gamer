#ifndef FLOW_FIELD_H
#define FLOW_FIELD_H

#include "Sprite.h"

/* Maze-aware chasing + stuck-enemy recovery (systems/FlowField.c). */
void FlowFieldUpdate(void) BANKED;                     /* StateGame UPDATE, every frame */
void EnemySteer(Sprite* e, INT8* dx, INT8* dy) BANKED;  /* replace greedy chase step */
/* Read-only: walking distance (tiles) from the player to this sprite's tile;
 * 254/255 = wall/unknown. Used by the e2e autopilot to judge real danger. */
UINT8 FlowFieldDistTo(Sprite* e) BANKED;

#endif
