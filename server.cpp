#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "TcpMessage.h"


#include <thread>
#include <iostream>
#include <sys/socket.h>

const int BUFFER_SIZE = 1032;//Maybe 1034?

using namespace std;

int main(int argc, char **argv) {
	if (argc != 3) {
		cerr << "usage: " << argv[0] << " PORT-NUMBER FILENAME" << '\n';
		return 1;
	}

	//We probably need default values, not sure what they are yet.
	string port = argv[1];
	string filename = argv[2];

	srand (time(NULL)); //Used to generate random ISN

	//Create UDP socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == -1) {
		perror("socket");
		return 1;
	}

	//Timeout flags and stuff could be set here
	int yes = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))== 1) {
		perror("setsockopt");
		return 1;
	}

	sockaddr_in addr, other;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(stoi(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr("127.0.0.1");

	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if(bind(sockfd, (sockaddr*) &addr, sizeof(addr)) == -1) {
		perror("bind");
		return 2;
	}

	int recv_length;
	char buf[BUFFER_SIZE];
	socklen_t other_length = sizeof(other);
	TcpMessage received;
	TcpMessage toSend;
	bool hasReceivedSyn = false;
	uint16_t synToSend = rand() % 65536;
	uint16_t ackToSend;

	for (int packetsSent = 0; packetsSent < 2; packetsSent++) {
		cout << "Waiting for something" << endl;
		recv_length = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (sockaddr *) &other, &other_length);
		if (recv_length == -1) {
			perror("recvfrom");
		}

		//Obtain header from receive
		received.bufferToMessage(buf, recv_length);

		cout << "Packet arrived from" << inet_ntoa(addr.sin_addr)<< ": " << ntohs(other.sin_port) << endl;
		cout << "Received:" << endl;
		//cout.write(buf, 8);
		//cout << endl;
		received.dump();


		//Send SYN-ACK if client is trying to set up connection
		//Should put into a switch statement once I figure out all the packet cases
		//Buffer manipulation is untested right now, but it compiles ¯\_(ツ)_/¯
		bool receivedSyn = received.getFlag('s');
	    bool receivedAck = received.getFlag('a');
		string flagsToSend = "";
		if (receivedSyn && receivedAck) {
			// error: both syn and ack were set by client
			// TODO
			cerr << "Both SYN and ACK were received!\n";
			exit(1);
		}
		else if (receivedSyn) {
			if (hasReceivedSyn) {
				// error: shouldn't receive syn twice from client
				// TODO
				cerr << "SYN received again!\n";
				exit(1);
			}
			flagsToSend = "SA";
			hasReceivedSyn = true;
		}
		else if (receivedAck) {
			if (!hasReceivedSyn) {
				// error: ack before syn
				// TODO
				cerr << "ACK received before SYN!\n";
				exit(1);
			}
			flagsToSend = "A";
		}

		if (packetsSent) {
			synToSend = received.ackNum;
		}
		ackToSend = received.seqNum + 1;
		toSend = TcpMessage(synToSend, ackToSend, 1034, flagsToSend);

		toSend.messageToBuffer(buf);
		cout << "sending" << endl;
		toSend.dump();

		//Send packets to client 
		int send_length = sendto(sockfd, buf, recv_length, 0, (sockaddr*) &other, other_length);
		if (send_length == -1) {
			perror("sendto");
		}
		//	sendto(sockfd, buf, recv_length, 0, (sockaddr*) &other, other_length);

	}

	close(sockfd);
	return 0;
}
