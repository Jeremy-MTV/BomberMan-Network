#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <curses.h>

#include "utils.h"
#include "structs.h"

#define BUFFER_SIZE                     1024

#define WINDOW_HEIGHT                   25
#define WINDOW_WIDTH                    90

#define BOTTOM_BAR_HEIGHT               3
#define BOTTOM_BAR_WIDTH                90

#define GAME_WIN_HEIGHT                 25
#define GAME_WIN_WIDTH                  50

#define GAME_SIDE_BAR_HEIGHT            25
#define GAME_SIDE_BAR_WIDTH             40

#define COLOR_PAIR_DEFAULT              1
#define COLOR_PAIR_BOTTOM_BAR           2
#define COLOR_PAIR_TITLE                3
#define COLOR_PAIR_SIDE_BAR             4
#define COLOR_PAIR_ROCK                 5
#define COLOR_PAIR_BRICK                6
#define COLOR_PAIR_BOMB                 7
#define COLOR_PAIR_EXPLOSION            8
#define COLOR_PAIR_PLAYER0              9
#define COLOR_PAIR_PLAYER1              10
#define COLOR_PAIR_PLAYER2              11
#define COLOR_PAIR_PLAYER3              12
#define COLOR_PAIR_PLAYER0_BOMB         13
#define COLOR_PAIR_PLAYER1_BOMB         14
#define COLOR_PAIR_PLAYER2_BOMB         15
#define COLOR_PAIR_PLAYER3_BOMB         16

#define ROCK_CHAR                       "   "
#define BRICK_CHAR                      "   "
#define BOMB_CHAR                       "   "
#define EXPLOSION_CHAR                  "   "
#define PLAYER0_CHAR                    " O "
#define PLAYER1_CHAR                    " O "
#define PLAYER2_CHAR                    " O "
#define PLAYER3_CHAR                    " O "
#define PLAYER0_BOMB_CHAR               " * "
#define PLAYER1_BOMB_CHAR               " * "
#define PLAYER2_BOMB_CHAR               " * "
#define PLAYER3_BOMB_CHAR               " * "

volatile sig_atomic_t isUdpRunning = 0;
struct in6_addr ServerAddress;
int ServerPort = 0;

int tcpSockFd = -1;
int udpSockFd = -1;
int currentState = 0;
int mode = 0;
int id = -1;
int team = -1;
int isRunning = 1;
uint16_t actionCount = 0;
uint16_t lastUpdateFullNum = -1;
uint16_t lastUpdateChangeNum = -1;

WINDOW* mainWin;
WINDOW* bottomBar;

WINDOW* gameWin;
WINDOW* gameSideBar;

pthread_mutex_t bottomBarMutex = PTHREAD_MUTEX_INITIALIZER;

uint8_t board[BOARD_SIZE][BOARD_SIZE];

void initBoard();

void disconnect(int signum);

int initTcpClient(const char* address, int port);

void* handleTcpMessage(void* arg);
void processServerMessage(const char* buffer, long length);
void processStartGame(const char* buffer, long length);

void* handleUdpMessage(void* arg);
void processUdpMessage(const char* buffer, long length);

void start();
void showMainWindow(char* menus[], int cursor, int menuNum);
void sendJoinGame(int mode);
void sendReadyGame();

void runGame();
void showBoard();
void showSideBar();
void sendAction(int action);

#endif // !CLIENT_H