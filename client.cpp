#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "TcpMessage.h"

using namespace std;
 
#define BUFFER_SIZE 512  //Max length of buffer
 
int main(int argc, char **argv)
{
    if (argc != 3) {
		cerr << "usage: " << argv[0] << " SERVER-HOST-OR-IP PORT-NUMBER" << '\n';
        exit(1);
    }

    string ip = argv[1];
    string port = argv[2];

	srand (time(NULL)); //Used to generate random ISN
 
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
		exit(1);
    }
 
    sockaddr_in si_server;
    memset((char *) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(stoi(port));
	socklen_t serverLen = sizeof(si_server);

    if (inet_aton(ip.c_str() , &si_server.sin_addr) == 0) {
        perror("inet_aton");
	    exit(1);
    }


	uint16_t synToSend = rand() % 65536;
   	uint16_t ackToSend = 0;
	uint16_t recvWindowToSend = 1034;
	size_t msgLen;
	char buffer[BUFFER_SIZE];
	TcpMessage packetToSend;


	/* send SYN */

	packetToSend = TcpMessage(synToSend, ackToSend, recvWindowToSend, "S");
	msgLen = packetToSend.messageToBuffer(buffer);
	cout << "sending SYN:" << endl;
	packetToSend.dump();
	if (sendto(sockfd, buffer, msgLen, 0, (sockaddr*)&si_server, serverLen) == -1) {
		perror("sendto");
		exit(1);
	}


	/* receive SYN-ACK */

	int recv_length = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
			(sockaddr*)&si_server, &serverLen);
	if (recv_length == -1) {
		perror("recvfrom");
		exit(1);
	}
	TcpMessage received(buffer, recv_length);
	cout << "receiving SYN-ACK:" << endl;
	received.dump();
	synToSend = received.ackNum;
	ackToSend = received.seqNum + 1;
	if (!received.getFlag('a') || !received.getFlag('s')) {
		// error: server responded, but without syn-ack
		// TODO
		cout << "Server responded, but without syn-ack!\n";
		exit(1);
	}


	/* send ACK */

	packetToSend = TcpMessage(synToSend, ackToSend, recvWindowToSend, "A");
	msgLen = packetToSend.messageToBuffer(buffer);
	cout << "sending ACK:" << endl;
	packetToSend.dump();
	if (sendto(sockfd, buffer, msgLen, 0, (sockaddr*)&si_server, serverLen) == -1) {
		perror("sendto");
		exit(1);
	}

 
    close(sockfd);
    return 0;
}
