#include <iostream>
#include <fstream>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <chrono>
#include <future>

#include "Utils.h"
#include "TcpMessage.h"

using namespace std;
 
const float TIMEOUT = 0.5f; // seconds

/*
template<typename T>
int waitFor(future<T>& promise) {
	chrono::seconds timer(TIMEOUT);
	// abort if request times out
	if (promise.wait_for(timer) == future_status::timeout) {
		cerr << "Connection timed out." << '\n';
		return -1;
	}
	return 0;
}
*/

#define OUTPUT_FILE "client-dump" // the file received and saved by the client
 
int main(int argc, char **argv)
{
    if (argc != 3) {
		cerr << "usage: " << argv[0] << " SERVER-HOST-OR-IP PORT-NUMBER" << '\n';
        exit(1);
    }

    string ip = argv[1];
    string port = argv[2];

	// Get IP address if we're given a hostname
	addrinfo hints;
	addrinfo* res;

	// prepare hints
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_STREAM; // TCP

	// get address
	int status = 0;
	if ((status = getaddrinfo(ip.c_str(), port.c_str(), &hints, &res)) != 0) {
		cerr << "getaddrinfo: " << gai_strerror(status) << '\n';
		exit(2);
	}

	addrinfo* p = res;
	// convert address to IPv4 address
	sockaddr_in* ipv4;
	char ipstr[INET_ADDRSTRLEN] = {'\0'};
	if (p != 0) {
		ipv4 = (sockaddr_in*)p->ai_addr;

		// convert the IP to a string
		inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
	}
	else {
		cerr << "IP address not found for " << ip << endl;
		exit(3);
	}
	ip = ipstr;
	cout << "IP: " << ip << "\n";

	srand (time(NULL)); //Used to generate random ISN
 
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
		exit(1);
    }

	// Set receive timeout of 0.5 s
	timeval recvTimeout;
	recvTimeout.tv_sec = 0;
	recvTimeout.tv_usec = 500000;

	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvTimeout, sizeof(recvTimeout)) == -1) {
		perror("setsockopt");
		return 1;
	}
 
    sockaddr_in si_server;
    memset((char *) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(stoi(port));
	socklen_t serverLen = sizeof(si_server);

    if (inet_aton(ip.c_str(), &si_server.sin_addr) == 0) {
        perror("inet_aton");
	    exit(1);
    }


	uint16_t seqToSend = rand() % MAX_SEQ_NUM;
   	uint16_t ackToSend = 0;
	uint16_t recvWindowToSend = INIT_RECV_WINDOW;
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
		//exit(1);
	}


	/* send ACK */

	seqToSend = incSeqNum(seqToSend, 1);// increase sequence number by 1
	ackToSend = incSeqNum(packetReceived.seqNum, 1);
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "A");
	cout << "sending ACK:" << endl;
	packetToSend.dump();
	packetToSend.sendto(sockfd, &si_server, serverLen);


	/* receive data */

	ofstream outFile(OUTPUT_FILE);

	TcpMessage delayedAck;
	bool delayedAckPending = false;

	while (true) {
		/* receive data packet */
		int r = packetReceived.recvfrom(sockfd, &si_server, serverLen);
		if (r == RECV_TIMEOUT) { // if timeout, try to recvfrom again
			if (delayedAckPending) { // send out a delayed ACK
				cout << "sending ACK:" << endl;
				packetToSend.dump();
				delayedAck.sendto(sockfd, &si_server, serverLen);
				delayedAckPending = false;
			}
			continue;
		}

		// else, r == RECV_SUCCESS
		cout << "receiving data:" << endl;
		packetReceived.dump();

		// FIN received
		if (packetReceived.getFlag('F'))
			break;

		// save data to file
		const char *data = packetReceived.data.c_str();
		streamsize dataSize = packetReceived.data.size();
		outFile.write(data, dataSize);

		// send cumulative ACK if a delayed ACK is pending
		if (delayedAckPending) {
			ackToSend = incSeqNum(packetReceived.seqNum, dataSize);
			delayedAck = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "A");
			delayedAckPending = true;

			cout << "sending ACK:" << endl;
			packetToSend.dump();
			delayedAck.sendto(sockfd, &si_server, serverLen);
			delayedAckPending = false;
		}
		else {
			/* prepare delayed ACK */
			ackToSend = incSeqNum(packetReceived.seqNum, dataSize);
			delayedAck = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "A");
			delayedAckPending = true;
		}

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

	/* Send FIN-ACK; seq # stays same b/c no payload*/
	ackToSend = incSeqNum(packetReceived.seqNum, 1);
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	cout << "Sending FIN-ACK to server\n";
	packetToSend.dump();
	
	/* Send FIN; seq # stays same b/c no payload */
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "F");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	cout << "Sending FIN to server\n";
	packetToSend.dump();

    /* Receive ACK of FIN without the FIN flag from server */ 
	packetReceived.recvfrom(sockfd, &si_server, serverLen);
	switch (packetReceived.flags) {
		case ACK_FLAG:
			//TODO: success
			cout << "Received ACK of FIN from server\n";
			break;
		default:
			cerr << "ACK of FIN wasn't received from server!\n";
			exit(1);
	}
	packetReceived.dump();
	close(sockfd);
	outFile.close();
	return 0;
}
