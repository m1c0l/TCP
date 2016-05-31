#ifndef UTILS_H
#define UTILS_H

const int BUFFER_SIZE = 1032;
const int HEADER_SIZE = 8;
const int DATA_SIZE = BUFFER_SIZE - HEADER_SIZE;

const uint16_t FIN_FLAG = 1;
const uint16_t SYN_FLAG = 2;
const uint16_t ACK_FLAG = 4;

#endif
