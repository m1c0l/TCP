#include "Utils.h"

// increment a seq or ack number and mod it by MAX_SEQ_NUM
uint16_t incSeqNum(uint16_t seq, uint16_t increment) {
	return (seq + increment) % MAX_SEQ_NUM;
}
