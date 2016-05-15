#include <cstdint>
#include <string>
using namespace std;

const uint8_t FIN_FLAG = 1;
const uint8_t SYN_FLAG = 2;
const uint8_t ACK_FLAG = 4;

struct TcpMessage {
	uint16_t sequenceNum = 0;
	uint16_t ackNum = 0;
	uint16_t receiverWindow = 0;
	const uint8_t reserved = 0; // 8 bits that are empty in the header
	uint8_t flags = 0;
	uint8_t sourcePort = 0;
	uint8_t destPort = 0;
        string data;

	bool setFlag(char flag);
};
