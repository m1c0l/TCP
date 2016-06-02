#include "TcpMessage.h"

#include <iostream>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace std;

TcpMessage::TcpMessage(uint16_t seq, uint16_t ack, uint16_t recvWind, string tcpFlags) {
	seqNum = seq;
	ackNum = ack;
	recvWindow = recvWind;
	setFlag(tcpFlags);
}

TcpMessage::TcpMessage() {}

bool TcpMessage::setFlag(string flag) {
    for(size_t i = 0; i<flag.size(); i++){
        switch(flag[i]) {
            case 'A':
            case 'a':
                flags |= ACK_FLAG;
                break;
            case 'F':
            case 'f':
                flags |= FIN_FLAG;
                break;
            case 'S':
            case 's':
                flags |= SYN_FLAG;
                break;
            default:
                cerr << "flag parameter must be A, F or S";
                return false;
        }
    }
    return true;
}

bool TcpMessage::getFlag(char flag) {
    switch(flag) {
        case 'A':
        case 'a':
            return !!(flags & ACK_FLAG);
        case 'F':
        case 'f':
            return !!(flags & FIN_FLAG);
        case 'S':
        case 's':
            return !!(flags & SYN_FLAG);
        default:
            cerr << "flag parameter must be A, F or S";
            return false;
    }
}

//Converts a char buffer from recvfrom into a TcpMessage object
void TcpMessage::bufferToMessage(uint8_t* buf, size_t size){
    seqNum = ((buf[0] << 8) + buf[1]);
    ackNum = ((buf[2] << 8) + buf[3]);
    recvWindow = ((buf[4] << 8) + buf[5]);
    flags =  buf[7] & 0xff;
    data.assign((char*)buf+8, size-8);
    return;
   
}

//Solves the halting problem
size_t TcpMessage::messageToBuffer(uint8_t* b) {
    
    //Probably a better way to do this...
    b[0] = (seqNum >> 8) & 0xff;
    b[1] = seqNum & 0xff;
    b[2] = (ackNum >> 8) & 0xff;
    b[3] = ackNum & 0xff;
    b[4] = recvWindow >> 8;
    b[5] = recvWindow & 0xff;
    b[6] = 0x0;
    b[7] = flags;
    
	data.copy((char*)b + 8, data.size(), 0);

	// 8 byte header + body
	return HEADER_SIZE + data.size();
}

void TcpMessage::sendto(int sockfd, sockaddr_in *si_other, socklen_t len) {
	uint8_t buf[BUFFER_SIZE];
	size_t msgLen = messageToBuffer(buf);
	if (::sendto(sockfd, buf, msgLen, 0, (sockaddr*)si_other, len) == -1) {
		perror("sendto");
		exit(1);
	}
}

void TcpMessage::recvfrom(int sockfd, sockaddr_in *si_other, socklen_t len) {
	uint8_t buf[BUFFER_SIZE];
	int recv_len = ::recvfrom(sockfd, buf, BUFFER_SIZE, 0, (sockaddr*)si_other, &len);
	if (recv_len == -1) {
		perror("recvfrom");
		exit(1);
	}
	bufferToMessage(buf, recv_len);
}

// Print out the TcpMessage's contents
void TcpMessage::dump() {
    cout << "seqNum = " << seqNum << '\t';
    cout << "ackNum = " << ackNum << '\t';
    cout << "recvWindow = " << recvWindow << endl;
    cout << "FIN_FLAG = " << getFlag('f') << '\t';
    cout << "SYN_FLAG = " << getFlag('s') << '\t';
    cout << "ACK_FLAG = " << getFlag('a') << endl;
    cout << "data = \"" << data << "\"" << endl;
    cout << endl;
}
