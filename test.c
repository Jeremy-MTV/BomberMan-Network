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

void* handleUdpConnection(void* arg) {
    int serverFd = *((int*) arg);

    struct sockaddr_in6 clientAddr;

    long receive;
    char buffer[1024];

    while (1) {
        printf("Waiting for UDP message\n");
        fflush(stdout);

        socklen_t addrLen = sizeof(clientAddr);
        receive = recvfrom(serverFd, buffer, 1024, 0, (struct sockaddr*) &clientAddr, &addrLen);
        char str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addrLen, str, INET6_ADDRSTRLEN);
        printf("UDP Received(%ld) from %s:%d\n", receive, str, clientAddr.sin6_port);
        fflush(stdout);

        if (receive > 0) {
            printf("Received: ");
            for (int i = 0; i < receive; i++) {
                printf("%02x ", buffer[i] & 0xFF);
            }
            printf("\n");
            fflush(stdout);
        } else {
            printf("ERROR: -1\n");
            fflush(stdout);
        }
    }
}

void* handleSend(void* arg) {
    int serverFd = *((int*) arg);

    struct sockaddr_in6 clientAddr;
    clientAddr.sin6_family = AF_INET6;
    clientAddr.sin6_addr = in6addr_any;
    clientAddr.sin6_port = htons(2208);

    while (1) {
        sleep(3);

        printf("Sent message\n");
        fflush(stdout);
        char message[] = "Hello, World!";
        sendto(serverFd, message, strlen(message), 0, (struct sockaddr*) &clientAddr, sizeof(clientAddr));
    }

    pthread_detach(pthread_self());
    return NULL;
}

int main() {
    int serverFd;
    struct sockaddr_in6 serverAddr, clientAddr;

    serverFd = socket(AF_INET6, SOCK_DGRAM, 0);
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_any;
    serverAddr.sin6_port = htons(2209);

    if (bind(serverFd, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) < 0) {
        perror("ERROR: Socket binding failed");
        return -1;
    }
    clientAddr.sin6_family = AF_INET6;
    clientAddr.sin6_addr = in6addr_any;
    clientAddr.sin6_port = htons(2208);


    pthread_t udpConnectionThread;
    pthread_create(&udpConnectionThread, NULL, &handleUdpConnection, &serverFd);

    pthread_t senderThread;
    pthread_create(&senderThread, NULL, &handleSend, &serverFd);

    while (1) {
        sleep(1);
    }

    return 0;
}
