#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "TcpMessage.h"

using namespace std;
 
#define BUFFER_SIZE 512  //Max length of buffer

#define OUTPUT_FILE "client-dump" // the file received and saved by the client
 
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


	uint16_t seqToSend = rand() % 65536;
   	uint16_t ackToSend = 0;
	uint16_t recvWindowToSend = 1034;
	size_t msgLen;
	uint8_t buffer[BUFFER_SIZE];
	TcpMessage packetToSend, packetReceived;


	/* send SYN */

	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "S");
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
	packetReceived = TcpMessage(buffer, recv_length);
	cout << "receiving SYN-ACK:" << endl;
	packetReceived.dump();
	if (!packetReceived.getFlag('a') || !packetReceived.getFlag('s')) {
		// error: server responded, but without syn-ack
		// TODO
		cout << "Server responded, but without syn-ack!\n";
		exit(1);
	}


	/* send ACK */

	seqToSend = packetReceived.ackNum;
	ackToSend = packetReceived.seqNum + 1;
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "A");
	msgLen = packetToSend.messageToBuffer(buffer);
	cout << "sending ACK:" << endl;
	packetToSend.dump();
	if (sendto(sockfd, buffer, msgLen, 0, (sockaddr*)&si_server, serverLen) == -1) {
		perror("sendto");
		exit(1);
	}


	/* receive data */

	ofstream outFile(OUTPUT_FILE);

	while (true) {
		/* receive data packet */
		recv_length = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
				(sockaddr*)&si_server, &serverLen);
		if (recv_length == -1) {
			perror("recvfrom");
			exit(1);
		}
		packetReceived = TcpMessage(buffer, recv_length);
		cout << "receiving data:" << endl;
		packetReceived.dump();

		// FIN received
		if (packetReceived.getFlag('F'))
			break;

		// save data to file
		const char *data = packetReceived.data.c_str();
		streamsize dataSize = packetReceived.data.size();
		outFile.write(data, dataSize);


		/* send ACK */

		seqToSend = packetReceived.ackNum;
		ackToSend = packetReceived.seqNum + dataSize + 1;
		packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "A");
		msgLen = packetToSend.messageToBuffer(buffer);
		cout << "sending ACK:" << endl;
		packetToSend.dump();
		if (sendto(sockfd, buffer, msgLen, 0, (sockaddr*)&si_server, serverLen) == -1) {
			perror("sendto");
			exit(1);
		}

	}

 
    /* Receive FIN from server */ 
	recv_length = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
			(sockaddr*)&si_server, &serverLen);
	if (recv_length == -1) {
		perror("recvfrom");
		exit(1);
	}
	packetReceived = TcpMessage(buffer, recv_length);
	switch (packetReceived.flags) {
		case FIN_FLAG:
			//TODO: success
			break;
		default:
			cerr << "FIN wasn't received from server!";
			exit(1);
	}

	/* Send FIN-ACK */
	seqToSend = packetReceived.ackNum;
	ackToSend = packetReceived.seqNum + 1;
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
	int hdrLen = packetToSend.messageToBuffer(buffer);
	if (sendto(sockfd, buffer, hdrLen, 0, (sockaddr*)&si_server, serverLen) == -1) {
		perror("sendto");
		exit(1);
	}
	
	/* Send FIN */
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "F");
	hdrLen = packetToSend.messageToBuffer(buffer);
	if (sendto(sockfd, buffer, hdrLen, 0, (sockaddr*)&si_server, serverLen) == -1) {
		perror("sendto");
		exit(1);
	}

    /* Receive FIN-ACK from server */ 
	recv_length = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
			(sockaddr*)&si_server, &serverLen);
	if (recv_length == -1) {
		perror("recvfrom");
		exit(1);
	}
	packetReceived = TcpMessage(buffer, recv_length);
	switch (packetReceived.flags) {
		case FIN_FLAG & ACK_FLAG:
			//TODO: success
			break;
		default:
			cerr << "FIN wasn't received from server!";
			exit(1);
	}
	close(sockfd);
	outFile.close();
	return 0;
}
