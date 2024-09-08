#ifndef SERVER_H
#define SERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#include "structs.h"

#define UDP_PORT                59998
#define GAME_START_PORT         60000

#define MAX_CLIENTS             200
#define BUFFER_SIZE             1024

struct in6_addr ServerAddress;

int tcpSockFd = 0;
int udpSockFd = 0;

static _Atomic uint16_t clientCount = 0;
Client* clients[MAX_CLIENTS];
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

static _Atomic uint16_t gameCount = 0;
Game* games[MAX_CLIENTS];
pthread_mutex_t gamesMutex = PTHREAD_MUTEX_INITIALIZER;

void initGames();

void* gameLoop(void* arg);

int initTcpServer(int port);
int initUdpServer(int port);
void* handleTcpConnection(void* arg);
void* handleUdpConnection(void* arg);
void addClient(Client* client);
void removeClient(int fd);
void* handleClient(void* arg);
void processClientMessage(const char* buffer, long length, int fd);
void processClientDisconnect(int fd);
void processJoinSoloGame(int fd);
void processJoinMultiGame(int fd);
void processReadySoloGame(int fd, int id, int team);
void processReadyTeamGame(int fd, int id, int team);
void processChatAll(int fd, const char* buffer, long length, int id, int team);
void processChatTeam(int fd, const char* buffer, long length, int id, int team);

#endif // !SERVER_H