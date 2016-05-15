#include "TcpMessage.h"

#include <iostream>
#include <stdlib.h>


bool TcpMessage::setFlag(char flag) {
	switch(flag) {
		case 'A':
			flags |= ACK_FLAG;
			break;
		case 'F':
			flags |= FIN_FLAG;
			break;
		case 'S':
			flags |= SYN_FLAG;
			break;
		default:
			cerr << "flag parameter must be A, F or S";
			exit(1);
	}
	return true;
}
