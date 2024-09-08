#include "player.h"

#include <stdlib.h>

Player* createPlayer(int socket, int id, int team) {
    Player* player = malloc(sizeof(Player));
    player->socket = socket;
    player->id = id;
    player->team = team;
    player->row = -1;
    player->col = -1;
    player->bomb = 0;
    player->status = PLAYER_STATUS_WAITING;
    return player;
}

void destroyPlayer(Player* player) {
    free(player);
    player = NULL;
}

void moveNorth(Player* player) {
    player->row--;
}


void moveSouth(Player* player) {
    player->row++;
}

void moveEast(Player* player) {
    player->col++;
}

void moveWest(Player* player) {
    player->col--;
}

void killPlayer(Player* player) {
    player->status = PLAYER_STATUS_DEAD;
}
