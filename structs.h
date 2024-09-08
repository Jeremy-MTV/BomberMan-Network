#ifndef STRUCTS_H
#define STRUCTS_H

#include <netinet/in.h>

#define NUM_PLAYERS                         4
#define BOARD_SIZE                          10
#define TIME_BROADCAST_FULL                 1000        // 1 second
#define TIME_BROADCAST_CHANGE               50          // 50 milliseconds
#define TIMEOUT_NO_ACTION                   90000       // 90 seconds
#define BOMB_TIME                           3000        // 3 seconds

#define MAX_NUM_ACTIONS                     8192
#define MAX_NUM_BROADCASTS                  65536

#define REQUEST_JOIN_SOLO                   1
#define REQUEST_JOIN_TEAM                   2
#define REQUEST_READY_SOLO                  3
#define REQUEST_READY_TEAM                  4
#define REQUEST_ACTION_SOLO                 5
#define REQUEST_ACTION_TEAM                 6
#define REQUEST_CHAT_ALL                    7
#define REQUEST_CHAT_TEAM                   8

#define RESPONSE_START_GAME_SOLO            9
#define RESPONSE_START_GAME_TEAM            10
#define RESPONSE_BOARD_FULL                 11
#define RESPONSE_BOARD_CHANGE               12
#define RESPONSE_CHAT_ALL                   13
#define RESPONSE_CHAT_TEAM                  14
#define RESPONSE_GAME_OVER_SOLO             15
#define RESPONSE_GAME_OVER_TEAM             16

#define ACTION_MOVE_NORTH                   0
#define ACTION_MOVE_EAST                    1
#define ACTION_MOVE_SOUTH                   2
#define ACTION_MOVE_WEST                    3
#define ACTION_BOMB                         4
#define ACTION_CANCEL_MOVE                  5

#define CELL_EMPTY                          0
#define CELL_ROCK                           1
#define CELL_BRICK                          2
#define CELL_BOMB                           3
#define CELL_EXPLOSION                      4
#define CELL_PLAYER0                        5
#define CELL_PLAYER1                        6
#define CELL_PLAYER2                        7
#define CELL_PLAYER3                        8
#define CELL_PLAYER0_BOMB                   9
#define CELL_PLAYER1_BOMB                   10
#define CELL_PLAYER2_BOMB                   11
#define CELL_PLAYER3_BOMB                   12

#define SERVER_ADDRESS4                     "127.0.0.1"
#define SERVER_ADDRESS6                     "ff02::1"

#define SERVER_PORT                     4096

typedef enum  {
    PLAYER_STATUS_WAITING,
    PLAYER_STATUS_READY,
    PLAYER_STATUS_PLAYING,
    PLAYER_STATUS_DEAD,
    PLAYER_STATUS_QUIT,
} PlayerStatus;

typedef struct {
    uint8_t** cells;
} Board;

typedef struct {
    int socket;
    int id;
    int team;
    int row;
    int col;
    uint64_t bomb;
    int bombRow;
    int bombCol;
    PlayerStatus status;
    struct sockaddr_in6 address;
} Player;


typedef enum {
    GAME_STATUS_WAITING,
    GAME_STATUS_PLAYING,
    GAME_STATUS_FINISHED,
} GameStatus;

typedef enum {
    GAME_TYPE_SOLO = 0,
    GAME_TYPE_TEAM,
} GameType;

typedef struct {
    uint16_t num;
    uint8_t action;
} Action;

typedef struct {
    Player* players[NUM_PLAYERS];
    Action* actionsMove[NUM_PLAYERS];
    Action* actionsBomb[NUM_PLAYERS];
    Board* board;
    GameType type;
    GameStatus status;
    int winner;
} Game;

typedef struct {
    struct sockaddr_in6 address;
    int sockFd;
} Client;

#endif // !STRUCTS_H
