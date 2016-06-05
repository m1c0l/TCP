#include <sys/socket.h>
#include <cstdlib>
#include <cstdio>
#include "Utils.h"
using namespace std;

// increment a seq or ack number and mod it by MAX_SEQ_NUM
uint16_t incSeqNum(uint16_t seq, uint16_t increment) {
	return (seq + increment) % MAX_SEQ_NUM;
}

// sets a socket's receive timer to sec/usec
void setSocketTimeout(int sockfd, int sec, int usec) {
	timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = usec;
	int r = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void*)&tv, sizeof(tv));
	if (r == -1) {
		perror("setsockopt");
		exit(1);
	}
}
