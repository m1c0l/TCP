#include <cstdint>
#include <string>
#include <sys/socket.h>
using namespace std;

const uint8_t FIN_FLAG = 1;
const uint8_t SYN_FLAG = 2;
const uint8_t ACK_FLAG = 4;

struct TcpMessage {
public:
	uint16_t seqNum = 0;
	uint16_t ackNum = 0;
	uint16_t recvWindow = 1034; //Double check this
	uint16_t flags = 0;
	//	uint8_t sourcePort = 0;
	//uint8_t destPort = 0;
	string data = "";

	TcpMessage(uint16_t seq, uint16_t ack, uint16_t recvWind, string tcpFlags);
	TcpMessage();
	bool setFlag(string flag);
	bool getFlag(char flag);
	void bufferToMessage(char* buf, int size);
	int messageToBuffer(char* b);
	void dump();
};
