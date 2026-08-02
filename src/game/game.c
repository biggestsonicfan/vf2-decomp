#include "vf2/game.h"

vf2_status vf2_game_initialize(vf2_game *game)
{
    if (game == 0) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    game->frame_number = 0u;
    game->initialized = 1;
    return VF2_OK;
}

vf2_status vf2_game_update(vf2_game *game)
{
    if (game == 0 || game->initialized == 0) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    ++game->frame_number;
    return VF2_OK;
}

void vf2_game_shutdown(vf2_game *game)
{
    if (game != 0) {
        game->initialized = 0;
    }
}
