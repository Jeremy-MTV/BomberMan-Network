#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <arpa/inet.h>

uint64_t getCurrentTimeMillis();

uint64_t getCurrentTimeMicros();

void printClientAddress(struct in6_addr addr);

void printMessage(const char* message, long length);

#endif // !UTILS_H
