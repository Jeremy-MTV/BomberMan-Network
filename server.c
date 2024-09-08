#include "server.h"

#include "utils.h"
#include "game.h"

void initGames() {
    inet_pton(AF_INET6, SERVER_ADDRESS6, &ServerAddress);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        games[i] = NULL;
    }
}

void* gameLoop(void* arg) {
    int isRunning = 1;

    uint64_t lastBroadcast = 0;
    uint64_t lastBroadcastFull = 0;

    uint16_t numBroadcast = 0;
    uint16_t numBroadcastFull = 0;

    Game* game = (Game*) arg;

    while (isRunning == 1) {
        if (game->status != GAME_STATUS_PLAYING) {
            usleep(TIME_BROADCAST_CHANGE * 1000);
            continue;
        }

        uint64_t currentTime = getCurrentTimeMillis();

        if (currentTime - lastBroadcastFull >= TIME_BROADCAST_FULL) {
            lastBroadcastFull = currentTime;
            lastBroadcast = currentTime;

            pthread_mutex_lock(&gamesMutex);

            processGame(game);

            uint16_t responseCodePacket = RESPONSE_BOARD_FULL << 3;
            responseCodePacket |= 0 << 1;
            responseCodePacket |= 0;

            uint16_t messageLength = 2 + 2 + 1 + 1 + BOARD_SIZE * BOARD_SIZE;
            char message[messageLength];

            message[0] = (char) ((responseCodePacket >> 8) & 0xFF);
            message[1] = (char) (responseCodePacket & 0xFF);

            message[2] = (char) ((numBroadcastFull >> 8) & 0xFF);
            message[3] = (char) (numBroadcastFull & 0xFF);

            message[4] = BOARD_SIZE & 0xFF;
            message[5] = BOARD_SIZE & 0xFF;

            for (int i = 0; i < BOARD_SIZE; ++i) {
                for (int j = 0; j < BOARD_SIZE; ++j) {
                    message[6 + i * BOARD_SIZE + j] = (char) game->board->cells[i][j];
                }
            }

            for (int i = 0; i < NUM_PLAYERS; ++i) {
                if (game->players[i] != NULL && game->players[i]->status != PLAYER_STATUS_QUIT) {
                    // Send udp message
                    sendto(udpSockFd, message, messageLength, 0,
                           (struct sockaddr*) &game->players[i]->address,
                           sizeof(game->players[i]->address));
                    // printf("%ld: Broadcasting board full %d\n", currentTime, ntohs(game->players[i]->address.sin6_port));
                    // fflush(stdout);
                }
            }

            pthread_mutex_unlock(&gamesMutex);

            printf("%ld: Broadcasting full board\n", currentTime);
            fflush(stdout);
            numBroadcastFull = (numBroadcastFull + 1) % MAX_NUM_BROADCASTS;
        } else if (currentTime - lastBroadcast >= TIME_BROADCAST_CHANGE) {
            lastBroadcast = currentTime;

            pthread_mutex_lock(&gamesMutex);

            Board* currentBoard = createBoard(0);
            copyBoard(currentBoard, game->board);
            processGame(game);
            uint8_t nb = 0;
            uint8_t* diff = getDiff(currentBoard, game->board, &nb);

            uint16_t responseCodePacket = RESPONSE_BOARD_CHANGE << 3;
            responseCodePacket |= 0 << 1;
            responseCodePacket |= 0;

            uint16_t messageLength = 2 + 2 + 1 + nb * 3;
            char message[messageLength];

            message[0] = (char) ((responseCodePacket >> 8) & 0xFF);
            message[1] = (char) (responseCodePacket & 0xFF);

            message[2] = (char) ((numBroadcast >> 8) & 0xFF);
            message[3] = (char) (numBroadcast & 0xFF);

            message[4] = (char) nb;
            for (int i = 0; i < nb; ++i) {
                message[5 + i * 3] = (char) diff[i * 3];
                message[5 + i * 3 + 1] = (char) diff[i * 3 + 1];
                message[5 + i * 3 + 2] = (char) diff[i * 3 + 2];
            }

            for (int i = 0; i < NUM_PLAYERS; ++i) {
                if (game->players[i] != NULL && game->players[i]->status != PLAYER_STATUS_QUIT) {
                    // Send udp message
                    sendto(udpSockFd, message, messageLength, 0,
                           (struct sockaddr*) &game->players[i]->address,
                           sizeof(game->players[i]->address));
                }
            }

            pthread_mutex_unlock(&gamesMutex);

            printf("%ld: Broadcasting board change\n", currentTime);
            fflush(stdout);
            numBroadcast = (numBroadcast + 1) % MAX_NUM_BROADCASTS;
        }

        if (game->status == GAME_STATUS_FINISHED) {
            // Send game finished tcp message to all players
            for (int i = 0; i < NUM_PLAYERS; ++i) {
                if (game->players[i] != NULL && game->players[i]->status != PLAYER_STATUS_QUIT) {
                    uint16_t responseCodePacket = 0;
                    if (game->type == GAME_TYPE_SOLO) {
                        responseCodePacket = RESPONSE_GAME_OVER_SOLO << 3;
                        responseCodePacket |= game->winner << 1;
                        responseCodePacket |= 0;
                    } else {
                        responseCodePacket = RESPONSE_GAME_OVER_TEAM << 3;
                        responseCodePacket |= 0 << 1;
                        responseCodePacket |= game->winner;
                    }

                    uint16_t messageLength = 2;
                    char message[messageLength];

                    message[0] = (char) ((responseCodePacket >> 8) & 0xFF);
                    message[1] = (char) (responseCodePacket & 0xFF);

                    send(game->players[i]->socket, message, messageLength, 0);
                }
            }

            printf("Game finished\n");
            fflush(stdout);
            isRunning = 0;
            continue;
        }

        // Sleep for TIME_BROADCAST_CHANGE - execution time
        uint64_t sleepTime = TIME_BROADCAST_CHANGE - (getCurrentTimeMillis() - currentTime);
        if (sleepTime > 0) {
            usleep(sleepTime * 1000);
        }
    }

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (games[i] != NULL && (games[i] == game)) {
            destroyGame(games[i]);
            games[i] = NULL;
            printf("Destroyed game %d\n", i);
            fflush(stdout);
            break;
        }
    }

    pthread_detach(pthread_self());
    return NULL;
}

int initTcpServer(int port) {
    int option = 1;
    int serverFd;
    struct sockaddr_in6 serverAddr;

    serverFd = socket(AF_INET6, SOCK_STREAM, 0);
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_any;
    serverAddr.sin6_port = htons(port);

    // signal(SIGPIPE, SIG_IGN);

    if (setsockopt(serverFd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*) &option, sizeof(option)) < 0) {
        perror("ERROR: setsockopt failed");
        return -1;
    }

    if (bind(serverFd, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("ERROR: Socket binding failed");
        return -1;
    }

    if (listen(serverFd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return -1;
    }

    return serverFd;
}

int initUdpServer(int port) {
    int option = 1;
    int serverFd;
    struct sockaddr_in6 serverAddr;

    serverFd = socket(AF_INET6, SOCK_DGRAM, 0);
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_any;
    serverAddr.sin6_port = htons(port);

    if (setsockopt(serverFd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*) &option, sizeof(option)) < 0) {
        perror("ERROR: setsockopt failed");
        return -1;
    }

    if (bind(serverFd, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("ERROR: Socket binding failed");
        return -1;
    }

    return serverFd;
}


void* tcpSender(void* arg) {
    int sockFd = *((int*) arg);

    while (1) {
        char message[2];
        message[0] = 0x11;
        message[1] = 0x22;
        send(sockFd, message, 2, 0);
        sleep(3);
    }

    pthread_detach(pthread_self());
    return NULL;
}

void* handleTcpConnection(void* arg) {
    int serverFd = *((int*) arg);

    int connectionFd;
    struct sockaddr_in6 clientAddr;
    pthread_t threadId;

    while (1) {
        socklen_t socklen = sizeof(clientAddr);
        connectionFd = accept(serverFd, (struct sockaddr*) &clientAddr, &socklen);

        if ((clientCount + 1) == MAX_CLIENTS) {
            printf("Max clients reached. Rejected: ");
            printClientAddress(clientAddr.sin6_addr);
            printf(":%d\n", clientAddr.sin6_port);
            fflush(stdout);
            close(connectionFd);
            continue;
        }

        Client* client = (Client*) malloc(sizeof(Client));
        client->address = clientAddr;
        client->sockFd = connectionFd;

        printf("ACCEPTED(%d): ", connectionFd);
        printClientAddress(clientAddr.sin6_addr);
        printf(":%d\n", clientAddr.sin6_port);

        addClient(client);
        pthread_create(&threadId, NULL, &handleClient, (void*) client);

        // TODO: Testing
        pthread_t tcpSendThread;
        if (pthread_create(&tcpSendThread, NULL, (void*) tcpSender, &connectionFd) != 0) {
            printf("ERROR: pthread\n");
        }

        /* Reduce CPU usage */
        usleep(1000);
    }
}

void* handleUdpConnection(void* arg) {
    int serverFd = *((int*) arg);

    struct sockaddr_in6 clientAddr;
    long receive;
    char buffer[BUFFER_SIZE];

    while (1) {
        socklen_t addrLen = sizeof(clientAddr);

        receive = recvfrom(serverFd, (char*) buffer, BUFFER_SIZE, 0, (struct sockaddr*) &clientAddr, &addrLen);
        if (receive > 0) {
            printf("UDP Received(%ld) from ", receive);
            printClientAddress(clientAddr.sin6_addr);
            printf(":%d: ", clientAddr.sin6_port);
            printMessage(buffer, receive);
            printf("\n");
            fflush(stdout);

            // Process UDP message
            if (receive < 4) {
                printf("Invalid request\n");
                fflush(stdout);
                continue;
            }

            uint16_t from = ntohs(clientAddr.sin6_port);
            uint16_t requestCodePacket = (buffer[0] << 8) | buffer[1];
            uint16_t requestCode = requestCodePacket >> 3;
            uint8_t id = requestCodePacket & 0x06;
            uint8_t eq = requestCodePacket & 0x01;
            uint16_t actionPacket = (buffer[2] << 8) | buffer[3];
            uint16_t num = actionPacket >> 3;
            uint8_t action = actionPacket & 0x07;

            int gameIndex = (from - GAME_START_PORT) / 4;
            int playerIndex = (from - GAME_START_PORT) % 4;

            if (gameIndex < 0 || gameIndex >= MAX_CLIENTS || games[gameIndex] == NULL ||
                games[gameIndex]->status != GAME_STATUS_PLAYING) {
                printf("Invalid game index\n");
                fflush(stdout);
                continue;
            }

            if (id != playerIndex || playerIndex < 0 || playerIndex >= NUM_PLAYERS ||
                games[gameIndex]->players[playerIndex] == NULL ||
                games[gameIndex]->players[playerIndex]->status != PLAYER_STATUS_PLAYING) {
                printf("Invalid player index\n");
                fflush(stdout);
                continue;
            }

            printf("From: %d, requestCode: %d, id: %d, eq: %d, num: %d, action: %d\n",
                   from, requestCode, id, eq, num, action);
            fflush(stdout);

            pthread_mutex_lock(&gamesMutex);

            Game* game = games[gameIndex];

            switch (action) {
                case ACTION_MOVE_NORTH:
                case ACTION_MOVE_EAST:
                case ACTION_MOVE_SOUTH:
                case ACTION_MOVE_WEST:
                    if (game->actionsMove[playerIndex] == NULL) {
                        game->actionsMove[playerIndex] = (Action*) malloc(sizeof(Action));
                    } else if (game->actionsMove[playerIndex]->num < num ||
                               (game->actionsMove[playerIndex]->num > (MAX_NUM_ACTIONS * 3 / 4) &&
                                num < (MAX_NUM_ACTIONS * 1 / 4))) {
                        free(game->actionsMove[playerIndex]);
                        game->actionsMove[playerIndex] = (Action*) malloc(sizeof(Action));
                    }
                    break;

                case ACTION_BOMB:
                    if (game->actionsBomb[playerIndex] == NULL) {
                        game->actionsBomb[playerIndex] = (Action*) malloc(sizeof(Action));
                    } else if (game->actionsBomb[playerIndex]->num < num ||
                               (game->actionsBomb[playerIndex]->num > (MAX_NUM_ACTIONS * 3 / 4) &&
                                num < (MAX_NUM_ACTIONS * 1 / 4))) {
                        free(game->actionsBomb[playerIndex]);
                        game->actionsBomb[playerIndex] = (Action*) malloc(sizeof(Action));
                    }
                    break;

                case ACTION_CANCEL_MOVE:
                    if (game->actionsMove[playerIndex] != NULL && game->actionsMove[playerIndex]->num == num) {
                        free(game->actionsMove[playerIndex]);
                        game->actionsMove[playerIndex] = NULL;
                    }
                    break;

                default:
                    printf("Invalid action\n");
                    fflush(stdout);
                    break;
            }

            pthread_mutex_unlock(&gamesMutex);
        } else {
            printf("ERROR: -1\n");
            fflush(stdout);
        }
    }
}

void addClient(Client* client) {
    pthread_mutex_lock(&clientsMutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] == NULL) {
            clients[i] = client;
            break;
        }
    }

    pthread_mutex_unlock(&clientsMutex);
}

void removeClient(int fd) {
    pthread_mutex_lock(&clientsMutex);

    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (clients[i] != NULL) {
            if (clients[i]->sockFd == fd) {
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&clientsMutex);
}

void* handleClient(void* arg) {
    char buffer[BUFFER_SIZE];
    uint8_t isExit = 0;

    clientCount++;
    Client* client = (Client*) arg;

    bzero(buffer, BUFFER_SIZE);

    while (1) {
        if (isExit) {
            break;
        }

        ssize_t receive = recv(client->sockFd, buffer, BUFFER_SIZE, 0);
        if (receive > 0) {
            processClientMessage(buffer, receive, client->sockFd);
        } else if (receive == 0) {
            printf("Client %d disconnected\n", client->sockFd);
            fflush(stdout);
            processClientDisconnect(client->sockFd);
            isExit = 1;
        } else {
            printf("ERROR: -1\n");
            fflush(stdout);
            isExit = 1;
        }

        bzero(buffer, BUFFER_SIZE);
    }

    close(client->sockFd);
    removeClient(client->sockFd);
    free(client);
    clientCount--;

    pthread_detach(pthread_self());
    return NULL;
}

void processClientMessage(const char* buffer, long length, int fd) {
    printf("Received(%zd) from %d: ", length, fd);
    printMessage(buffer, length);
    printf("\n");
    fflush(stdout);

    uint16_t requestCodePacket = (buffer[0] << 8) | buffer[1];
    uint16_t requestCode = requestCodePacket >> 3;
    uint8_t id = requestCodePacket & 0x06;
    uint8_t eq = requestCodePacket & 0x01;

    switch (requestCode) {
        case REQUEST_JOIN_SOLO:
            printf("%d> REQUEST_JOIN_SOLO\n", fd);
            fflush(stdout);
            processJoinSoloGame(fd);
            break;

        case REQUEST_JOIN_TEAM:
            printf("%d> REQUEST_JOIN_TEAM\n", fd);
            fflush(stdout);
            processJoinMultiGame(fd);
            break;

        case REQUEST_READY_SOLO:
            printf("%d> REQUEST_READY_SOLO\n", fd);
            fflush(stdout);
            processReadySoloGame(fd, id, eq);
            break;

        case REQUEST_READY_TEAM:
            printf("%d> REQUEST_READY_TEAM\n", fd);
            fflush(stdout);
            processReadyTeamGame(fd, id, eq);
            break;

        case REQUEST_CHAT_ALL:
            printf("%d> REQUEST_CHAT_ALL\n", fd);
            fflush(stdout);
            processChatAll(fd, buffer, length, id, eq);
            break;

        case REQUEST_CHAT_TEAM:
            printf("%d> REQUEST_CHAT_TEAM\n", fd);
            fflush(stdout);
            processChatTeam(fd, buffer, length, id, eq);
            break;

        default:
            break;
    }
}

void processClientDisconnect(int fd) {
    char string[100];
    sprintf(string, "%d has left\n", fd);
    printf("%s", string);
    fflush(stdout);

    pthread_mutex_lock(&gamesMutex);

    uint8_t isGameFound = 0;
    for (int i = 0; i < MAX_CLIENTS && isGameFound == 0; ++i) {
        if (games[i] != NULL) {
            for (int j = 0; j < NUM_PLAYERS; ++j) {
                if (games[i]->players[j] != NULL && games[i]->players[j]->socket == fd) {
                    games[i]->players[j]->status = PLAYER_STATUS_QUIT;
                    if (games[i]->status == GAME_STATUS_WAITING) {
                        printf("Destroying player %d, %d\n", i, j);
                        fflush(stdout);
                        destroyPlayer(games[i]->players[j]);
                        games[i]->players[j] = NULL;
                    }
                    break;
                }
            }
        }
    }

    pthread_mutex_unlock(&gamesMutex);
}

void processJoinSoloGame(int fd) {
    pthread_mutex_lock(&gamesMutex);

    // Check if player already in game
    Game* game = NULL;
    int index = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (games[i] != NULL) {
            if (game == NULL && games[i]->type == GAME_TYPE_SOLO) {
                uint8_t isGameFull = 1;
                for (int j = 0; j < NUM_PLAYERS; ++j) {
                    if (games[i]->players[j] == NULL) {
                        isGameFull = 0;
                        break;
                    }
                }

                if (isGameFull == 0) {
                    game = games[i];
                    index = i;
                }
            }

            for (int j = 0; j < NUM_PLAYERS; ++j) {
                if (games[i]->players[j] != NULL && games[i]->players[j]->socket == fd &&
                    games[i]->players[j]->status != PLAYER_STATUS_QUIT) {
                    printf("%d> ERROR: Player already in game\n", fd);
                    fflush(stdout);
                    pthread_mutex_unlock(&gamesMutex);
                    return;
                }
            }
        }
    }

    if (game == NULL) {
        printf("Creating solo game\n");
        fflush(stdout);
        game = createGame(GAME_TYPE_SOLO);
        pthread_t threadId;
        pthread_create(&threadId, NULL, &gameLoop, (void*) game);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (games[i] == NULL) {
                games[i] = game;
                index = i;
                break;
            }
        }
    }

    Player* player = NULL;
    for (int j = 0; j < NUM_PLAYERS; ++j) {
        if (game->players[j] == NULL) {
            player = createPlayer(fd, j, 0);
            player->address.sin6_family = AF_INET6;
            player->address.sin6_addr = in6addr_any;
            player->address.sin6_port = htons(GAME_START_PORT + index * 4 + j);
            game->players[j] = player;
            break;
        }
    }

    if (player == NULL) {
        printf("%d> ERROR: Cannot create player\n", fd);
        fflush(stdout);
        pthread_mutex_unlock(&gamesMutex);
        return;
    }

    for (int i = 0; i < NUM_PLAYERS; ++i) {
        if (game->players[i] != NULL) {
            printf("%d ", game->players[i]->socket);
        } else {
            printf("NULL ");
        }
    }
    printf("\n");

    char message[22];

    uint16_t responseCodePacket = RESPONSE_START_GAME_SOLO << 3;
    responseCodePacket |= player->id << 1;
    responseCodePacket |= player->team;

    uint16_t portUdp = UDP_PORT;
    uint16_t portMDiff = ntohs(player->address.sin6_port);

    message[0] = (char) ((responseCodePacket >> 8) & 0xFF);
    message[1] = (char) (responseCodePacket & 0xFF);
    message[2] = (char) ((portUdp >> 8) & 0xFF);
    message[3] = (char) (portUdp & 0xFF);
    message[4] = (char) ((portMDiff >> 8) & 0xFF);
    message[5] = (char) (portMDiff & 0xFF);
    for (int i = 0; i < 16; ++i) {
        message[6 + i] = ((char*) &ServerAddress)[i];
    }

    send(fd, message, 22, 0);

    printf("%d> Joined solo game %d, %d\n", fd, portUdp, portMDiff);
    fflush(stdout);

    pthread_mutex_unlock(&gamesMutex);
}

void processJoinMultiGame(int fd) {
    pthread_mutex_lock(&gamesMutex);

    // Check if player already in game
    Game* game = NULL;
    int index = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (games[i] != NULL) {
            if (game == NULL && games[i]->type == GAME_TYPE_TEAM) {
                uint8_t isGameFull = 1;
                for (int j = 0; j < NUM_PLAYERS; ++j) {
                    if (games[i]->players[j] == NULL) {
                        isGameFull = 0;
                        break;
                    }
                }

                if (isGameFull == 0) {
                    game = games[i];
                    index = i;
                }
            }

            for (int j = 0; j < NUM_PLAYERS; ++j) {
                if (games[i]->players[j] != NULL && games[i]->players[j]->socket == fd &&
                    games[i]->players[j]->status != PLAYER_STATUS_QUIT) {
                    printf("%d> ERROR: Player already in game\n", fd);
                    fflush(stdout);
                    pthread_mutex_unlock(&gamesMutex);
                    return;
                }
            }
        }
    }

    if (game == NULL) {
        printf("Creating team game\n");
        fflush(stdout);
        game = createGame(GAME_TYPE_TEAM);
        pthread_t threadId;
        pthread_create(&threadId, NULL, &gameLoop, (void*) game);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (games[i] == NULL) {
                games[i] = game;
                index = i;
                break;
            }
        }
    }

    Player* player = NULL;
    for (int j = 0; j < NUM_PLAYERS; ++j) {
        if (game->players[j] == NULL) {
            player = createPlayer(fd, j, j % 2);
            player->address.sin6_family = AF_INET6;
            player->address.sin6_addr = in6addr_any;
            player->address.sin6_port = htons(GAME_START_PORT + index * 4 + j);
            game->players[j] = player;
            break;
        }
    }

    if (player == NULL) {
        printf("%d> ERROR: Cannot create player\n", fd);
        fflush(stdout);
        pthread_mutex_unlock(&gamesMutex);
        return;
    }

    for (int i = 0; i < NUM_PLAYERS; ++i) {
        if (game->players[i] != NULL) {
            printf("%d ", game->players[i]->socket);
        } else {
            printf("NULL ");
        }
    }
    printf("\n");

    char message[22];

    uint16_t responseCodePacket = RESPONSE_START_GAME_TEAM << 3;
    responseCodePacket |= player->id << 1;
    responseCodePacket |= player->team;

    uint16_t portUdp = UDP_PORT;
    uint16_t portMDiff = ntohs(player->address.sin6_port);

    message[0] = (char) ((responseCodePacket >> 8) & 0xFF);
    message[1] = (char) (responseCodePacket & 0xFF);
    message[2] = (char) ((portUdp >> 8) & 0xFF);
    message[3] = (char) (portUdp & 0xFF);
    message[4] = (char) ((portMDiff >> 8) & 0xFF);
    message[5] = (char) (portMDiff & 0xFF);
    for (int i = 0; i < 16; ++i) {
        message[6 + i] = ((char*) &ServerAddress)[i];
    }

    send(fd, message, 22, 0);

    printf("%d> Joined team game\n", fd);
    fflush(stdout);

    pthread_mutex_unlock(&gamesMutex);
}

void processReadySoloGame(int fd, int id, int team) {
    pthread_mutex_lock(&gamesMutex);

    Game* game = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (games[i] != NULL) {
            for (int j = 0; j < NUM_PLAYERS; ++j) {
                if (games[i]->players[j] != NULL && games[i]->players[j]->socket == fd) {
                    if (games[i]->players[j]->status == PLAYER_STATUS_WAITING) {
                        games[i]->players[j]->status = PLAYER_STATUS_READY;
                        game = games[i];
                    }
                    break;
                }
            }
        }
    }

    printf("%d> Ready\n", fd);
    fflush(stdout);

    if (game != NULL && game->type == GAME_TYPE_SOLO) {
        for (int i = 0; i < NUM_PLAYERS; ++i) {
            if (game->players[i] == NULL || game->players[i]->status == PLAYER_STATUS_WAITING) {
                pthread_mutex_unlock(&gamesMutex);
                return;
            }
        }

        startGame(game);
        game->status = GAME_STATUS_PLAYING;
        printf("Game started\n");
        fflush(stdout);
    }

    pthread_mutex_unlock(&gamesMutex);
}

void processReadyTeamGame(int fd, int id, int team) {
    pthread_mutex_lock(&gamesMutex);

    Game* game = NULL;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (games[i] != NULL) {
            for (int j = 0; j < NUM_PLAYERS; ++j) {
                if (games[i]->players[j] != NULL && games[i]->players[j]->socket == fd) {
                    if (games[i]->players[j]->status == PLAYER_STATUS_WAITING) {
                        games[i]->players[j]->status = PLAYER_STATUS_READY;
                        game = games[i];
                    }
                    break;
                }
            }
        }
    }

    printf("%d> Ready\n", fd);
    fflush(stdout);

    if (game != NULL && game->type == GAME_TYPE_TEAM) {
        for (int i = 0; i < NUM_PLAYERS; ++i) {
            if (game->players[i] != NULL && game->players[i]->status == PLAYER_STATUS_WAITING) {
                pthread_mutex_unlock(&gamesMutex);
                return;
            }
        }

        startGame(game);
        printf("Game started\n");
        fflush(stdout);
    }

    pthread_mutex_unlock(&gamesMutex);
}

void processChatAll(int fd, const char* buffer, long length, int id, int team) {
    uint8_t messageLength = buffer[2];
    if (messageLength > length - 3) {
        printf("%d> ERROR: Invalid message length (%d/%ld)\n", fd, messageLength, length);
        fflush(stdout);
        return;
    }

    uint16_t responseCodePacket = RESPONSE_CHAT_ALL << 3;
    responseCodePacket |= id << 1;
    responseCodePacket |= team;

    char message[length];
    message[0] = (char) ((responseCodePacket >> 8) & 0xFF);
    message[1] = (char) (responseCodePacket & 0xFF);
    message[2] = (char) messageLength;
    memcpy(message + 3, buffer + 3, messageLength);

    printf("%d> chat all: %s\n", fd, message);
    fflush(stdout);

    pthread_mutex_lock(&clientsMutex);

    Game* game = NULL;
    for (int i = 0; i < MAX_CLIENTS && game == NULL; ++i) {
        if (games[i] != NULL) {
            for (int j = 0; j < NUM_PLAYERS; ++j) {
                if (games[i]->players[j] != NULL && games[i]->players[j]->socket != fd &&
                    games[i]->players[j]->status != PLAYER_STATUS_QUIT) {
                    game = games[i];
                    break;
                }
            }
        }
    }

    if (game != NULL) {
        for (int i = 0; i < NUM_PLAYERS; ++i) {
            if (game->players[i] != NULL && game->players[i]->socket != fd &&
                game->players[i]->status != PLAYER_STATUS_QUIT) {
                send(game->players[i]->socket, message, length, 0);
            }
        }
    }

    pthread_mutex_unlock(&clientsMutex);
}

void processChatTeam(int fd, const char* buffer, long length, int id, int team) {
    uint8_t messageLength = buffer[2];
    if (messageLength > length - 3) {
        printf("%d> ERROR: Invalid message length (%d/%ld)\n", fd, messageLength, length);
        fflush(stdout);
        return;
    }

    uint16_t responseCodePacket = RESPONSE_CHAT_TEAM << 3;
    responseCodePacket |= id << 1;
    responseCodePacket |= team;

    char message[length];
    message[0] = (char) ((responseCodePacket >> 8) & 0xFF);
    message[1] = (char) (responseCodePacket & 0xFF);
    message[2] = (char) messageLength;
    memcpy(message + 3, buffer + 3, messageLength);

    printf("%d> chat team: %s\n", fd, message);
    fflush(stdout);

    pthread_mutex_lock(&clientsMutex);

    Game* game = NULL;
    Player* player = NULL;
    for (int i = 0; i < MAX_CLIENTS && game == NULL; ++i) {
        if (games[i] != NULL) {
            for (int j = 0; j < NUM_PLAYERS; ++j) {
                if (games[i]->players[j] != NULL && games[i]->players[j]->socket == fd &&
                    games[i]->players[j]->status != PLAYER_STATUS_QUIT) {
                    game = games[i];
                    player = games[i]->players[j];
                    break;
                }
            }
        }
    }

    if (game != NULL && player != NULL) {
        for (int i = 0; i < NUM_PLAYERS; ++i) {
            if (game->players[i] != NULL && game->players[i]->socket != fd &&
                game->players[i]->status != PLAYER_STATUS_QUIT && game->players[i]->team == player->team) {
                send(game->players[i]->socket, message, length, 0);
            }
        }
    }

    pthread_mutex_unlock(&clientsMutex);
}

int main(int argc, char* argv[]) {
    int tcpServer = initTcpServer(SERVER_PORT);
    if (tcpServer < 0) {
        return EXIT_FAILURE;
    }

    int udpServer = initUdpServer(UDP_PORT);
    if (udpServer < 0) {
        return EXIT_FAILURE;
    }

    initGames();

    printClientAddress(ServerAddress);
    printf(" -> %ld\n", getCurrentTimeMillis());
    fflush(stdout);

    printf("Bomberman server started on port %d\n", SERVER_PORT);
    fflush(stdout);

    pthread_t udpConnectionThread;
    pthread_create(&udpConnectionThread, NULL, &handleUdpConnection, &udpServer);

    pthread_t tcpConnectionThread;
    pthread_create(&tcpConnectionThread, NULL, &handleTcpConnection, &tcpServer);
    pthread_join(tcpConnectionThread, NULL);

    return EXIT_SUCCESS;
}