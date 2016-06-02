#ifndef UTILS_H
#define UTILS_H

#include <cstdint>

const int BUFFER_SIZE = 1032;
const int INIT_CONGEST_WINDOW = 1024;
const int HEADER_SIZE = 8;
const int DATA_SIZE = BUFFER_SIZE - HEADER_SIZE;
const uint16_t MAX_SEQ_NUM = 30720;
const uint16_t INIT_RECV_WINDOW = 30720;

const uint16_t FIN_FLAG = 1;
const uint16_t SYN_FLAG = 2;
const uint16_t ACK_FLAG = 4;


uint16_t incSeqNum(uint16_t seq, uint16_t increment);

#endif
