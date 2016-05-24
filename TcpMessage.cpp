#include "TcpMessage.h"

#include <iostream>
#include <stdlib.h>

using namespace std;

const int BUFFER_SIZE = 1024;


TcpMessage::TcpMessage(uint16_t seq, uint16_t ack, uint16_t recvWind,
		string tcpFlags) {
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
void TcpMessage::bufferToMessage(char* buf, int size){
    // struct TcpMessage recieved;
    seqNum = ((buf[0] << 8) + buf[1]);
    ackNum = ((buf[2] << 8) + buf[3]);
    recvWindow = ((buf[4] << 8) + buf[5]);
    flags =  buf[7] & 0xff;
    //sourcePort = buf[8];
    //    destPort = buf[9];
    //data.assign(buf+8, size-8);
    //data.copy(buffer, size, 10);
    return;   
   
}

//Solves the halting problem
void TcpMessage::messageToBuffer(char* b){
    
    //Probably a better way to do this...
    b[0] = (seqNum >> 8) & 0xff;
    b[1] = seqNum & 0xff;
    b[2] = (ackNum >> 8) & 0xff;
    b[3] = ackNum & 0xff;
    b[4] = recvWindow >> 8;
    b[5] = recvWindow & 0xff;
    b[6] = 0x0;
    b[7] = flags;
    //b[8] = sourcePort;
    //b[9] = destPort;
    
    //if (data != "")
    //	data.copy(b, 1024, 8);
	//return;
    
}

// Print out the TcpMessage's contents
void TcpMessage::dump() {
    cout << "seqNum = " << seqNum << endl;
    cout << "ackNum = " << ackNum << endl;
    cout << "recvWindow = " << recvWindow << endl;
    cout << "FIN_FLAG = " << getFlag('f') << endl;
    cout << "SYN_FLAG = " << getFlag('s') << endl;
    cout << "ACK_FLAG = " << getFlag('a') << endl;
    cout << "data = \"" << data << "\"" << endl;
}
