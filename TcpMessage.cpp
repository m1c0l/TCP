#include "TcpMessage.h"

#include <iostream>
#include <stdlib.h>

const int BUFFER_SIZE = 1024;

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

//Converts a char buffer from recvfrom into a TcpMessage object
void TcpMessage::bufferToMessage(char* buf, int size){
    // struct TcpMessage recieved;
    seqNum = ((buf[0] * 256) + buf[1]);
    ackNum = ((buf[2] * 256) + buf[3]);
    recvWindow = ((buf[4] * 256) + buf[5]);
    flags =  buf[7];
    //sourcePort = buf[8];
    //    destPort = buf[9];
    //data.assign(buf+8, size-8);
    //data.copy(buffer, size, 10);
    return;   
   
}

//Solves the halting problem
void TcpMessage::messageToBuffer(char* b){
    
    //Probably a better way to do this...
    b[0] = (seqNum >> 8) & 0xf;
    b[1] = seqNum & 0xf;
    b[2] = (ackNum >> 8) & 0xf;
    b[3] = ackNum & 0xf;
    b[4] = recvWindow >> 8;
    b[5] = recvWindow & 0xf;
    b[6] = 0x0;
    b[7] = flags;
    //b[8] = sourcePort;
    //b[9] = destPort;
    if (data != "")
	data.copy(b, 1024, 8);
    return;
    
}




