#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "Print.h"

extern UINT16 ready_coins; // Player's currency

void START() {
    THIS->custom_data[CD_DOOR_STATE] = 0; // Closed by default
    /* Persist on wide maps where the door starts off-camera. */
    THIS->lim_x = 255;
    THIS->lim_y = 255;
}

void UPDATE() {
}

void DESTROY() {
    // Cleanup if needed
} 