#ifndef TCPMESSAGE_H
#define TCPMESSAGE_H

#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "Utils.h"

using namespace std;

struct TcpMessage {
public:
	uint16_t seqNum = BAD_SEQ_NUM;
	uint16_t ackNum = BAD_SEQ_NUM;
	uint16_t recvWindow = INIT_RECV_WINDOW;
	uint16_t flags = 0;
	string data = "";

	TcpMessage(uint16_t seq, uint16_t ack, uint16_t recvWind, string tcpFlags);
	TcpMessage();
	bool setFlag(string flag);
	bool getFlag(char flag);
	void bufferToMessage(uint8_t* buf, size_t size);
	size_t messageToBuffer(uint8_t* b);
	void sendto(int sockfd, sockaddr_in *si_other, socklen_t len);
	int recvfrom(int sockfd, sockaddr_in *si_other, socklen_t len);
	void dump();
};

#endif /* TCPMESSAGE_H */
