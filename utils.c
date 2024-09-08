#include "utils.h"

uint64_t getCurrentTimeMillis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

uint64_t getCurrentTimeMicros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

void printMessage(const char* message, long length)  {
    for (int i = 0; i < length; i++) {
        printf("%02x ", message[i] & 0xFF);
    }
}

void printClientAddress(struct in6_addr addr) {
    char str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr, str, INET6_ADDRSTRLEN);
    printf("%s", str);
}
