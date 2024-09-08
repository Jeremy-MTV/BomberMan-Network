#ifndef PLAYER_H
#define PLAYER_H

#include "structs.h"

Player* createPlayer(int socket, int id, int team);
void destroyPlayer(Player* player);
void moveNorth(Player* player);
void moveEast(Player* player);
void moveSouth(Player* player);
void moveWest(Player* player);
void killPlayer(Player* player);

#endif // !PLAYER_H
