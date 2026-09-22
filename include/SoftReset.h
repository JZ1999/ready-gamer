#ifndef SOFT_RESET_H
#define SOFT_RESET_H

#include <gb/gb.h>
#include "Keys.h"

/* A real cartridge/console has no reset button, so holding A+B+START+SELECT
 * performs a warm reset (GBDK reset(): restarts crt0 keeping the CPU type).
 * Call at the top of UPDATE() in gameplay states. */
#define SOFT_RESET_MASK (J_A | J_B | J_START | J_SELECT)
#define CHECK_SOFT_RESET()     do { if ((keys & SOFT_RESET_MASK) == SOFT_RESET_MASK) { reset(); } } while (0)

#endif
