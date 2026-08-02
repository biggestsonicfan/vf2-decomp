#ifndef VF2_GAME_H
#define VF2_GAME_H

#include "vf2/status.h"

typedef struct vf2_game {
    unsigned frame_number;
    int initialized;
} vf2_game;

vf2_status vf2_game_initialize(vf2_game *game);
vf2_status vf2_game_update(vf2_game *game);
void vf2_game_shutdown(vf2_game *game);

#endif
