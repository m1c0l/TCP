#ifndef UTILS_H
#define UTILS_H

#include <cstdint>
#include <ctime>

const int BUFFER_SIZE = 1032;
const int INIT_CONGEST_WINDOW = 1024;
const int HEADER_SIZE = 8;
const int DATA_SIZE = BUFFER_SIZE - HEADER_SIZE;
const uint16_t MAX_SEQ_NUM = 30720;
const uint16_t INIT_RECV_WINDOW = (MAX_SEQ_NUM + 1) / 2;

const uint16_t FIN_FLAG = 1;
const uint16_t SYN_FLAG = 2;
const uint16_t ACK_FLAG = 4;

const int RECV_SUCCESS = 1;
const int RECV_TIMEOUT = 2;

const int TIMEOUT = 500; // milliseconds
const int MAX_SEG_LIFETIME = 60 * 1000; // milliseconds

const uint16_t BAD_SEQ_NUM = 55555;

uint16_t incSeqNum(uint16_t seq, uint16_t increment);
bool inWindow(uint16_t seq, uint16_t bot, uint16_t top);
void setSocketTimeout(int sockfd, timeval tv);
timeval now();
timeval timeRemaining(timeval start);
bool timeval_cmp(timeval a, timeval b);

#endif
