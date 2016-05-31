#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "TcpMessage.h"

using namespace std;
 
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


	uint16_t seqToSend = rand() % 0xffff;
   	uint16_t ackToSend = 0;
	uint16_t recvWindowToSend = 1034;
	TcpMessage packetToSend, packetReceived;


	/* send SYN */

	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "S");
	cout << "sending SYN:" << endl;
	packetToSend.dump();
	packetToSend.sendto(sockfd, &si_server, serverLen);


	/* receive SYN-ACK */

	packetReceived.recvfrom(sockfd, &si_server, serverLen);
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
	cout << "sending ACK:" << endl;
	packetToSend.dump();
	packetToSend.sendto(sockfd, &si_server, serverLen);


	/* receive data */

	ofstream outFile(OUTPUT_FILE);

	while (true) {
		/* receive data packet */
		cout << "receiving data:" << endl;
		packetReceived.recvfrom(sockfd, &si_server, serverLen);
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
		cout << "sending ACK:" << endl;
		packetToSend.dump();
		packetToSend.sendto(sockfd, &si_server, serverLen);

	}

 
    /* Receive FIN from server */ 
	/*packetReceived.recvfrom(sockfd, &si_server, serverLen);
	switch (packetReceived.flags) {
		case FIN_FLAG:
			//TODO: success
			break;
		default:
			cerr << "FIN wasn't received from server!";
			exit(1);
	}*/

	/* Send FIN-ACK */
	seqToSend = packetReceived.ackNum;
	ackToSend = packetReceived.seqNum + 1;
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	cout << "Sending FIN-ACK to server\n";
	packetToSend.dump();
	
	/* Send FIN */
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "F");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	cout << "Sending FIN to server\n";
	packetToSend.dump();

    /* Receive FIN-ACK from server */ 
	packetReceived.recvfrom(sockfd, &si_server, serverLen);
	switch (packetReceived.flags) {
		case FIN_FLAG | ACK_FLAG:
			//TODO: success
			cout << "Received FIN-ACK from server\n";
			break;
		default:
			cerr << "FIN-ACK wasn't received from server!\n";
			exit(1);
	}
	packetReceived.dump();
	close(sockfd);
	outFile.close();
	return 0;
}
