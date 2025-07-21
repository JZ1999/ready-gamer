#include "Banks/SetAutoBank.h"
#include "ZGBMain.h"
#include "SpriteManager.h"
#include "SpriteData.h"
#include "Print.h"

extern UINT16 ready_coins; // Player's currency

void START() {
    THIS->custom_data[CD_DOOR_STATE] = 0; // Closed by default
}

void UPDATE() {
    // If door is open, do nothing
    if (THIS->custom_data[CD_DOOR_STATE]) return;

    UINT8 cost = THIS->custom_data[CD_DOOR_COST];

    // Check for player collision
    UINT8 i;
    Sprite* spr;
    SPRITEMANAGER_ITERATE(i, spr) {
        if (spr->type == SpritePlayer && CheckCollision(THIS, spr)) {
            // If player has enough coins, open the door
            if (ready_coins >= cost) {
                ready_coins -= cost;
                THIS->custom_data[CD_DOOR_STATE] = 1; // Open
                SpriteManagerRemove(THIS_IDX);
                Printf("Door opened!\n");
            } else {
                Printf("Need %d Ready Coins!\n", cost);
            }
        }
    }
}

void DESTROY() {
    // Cleanup if needed
} 