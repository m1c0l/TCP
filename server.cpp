#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>
#include <thread>
#include <iostream>
#include <chrono>
#include <future>
#include <unordered_map>

#include "Utils.h"
#include "TcpMessage.h"

using namespace std;


bool keepGettingAcks = true;
uint16_t lastAckRecvd;

void getAcksHelper(uint16_t finalAck, int sockfd, sockaddr_in *si_other, socklen_t len) {
	TcpMessage ack;
	while (keepGettingAcks && lastAckRecvd != finalAck) {
		if (ack.recvfrom(sockfd, si_other, len) == RECV_SUCCESS) {
			cout << "received ACK:" << endl;
			ack.dump();
			lastAckRecvd = ack.ackNum;
		}
		else {
			cout << "no ACK received" << endl;
		}
	}
}

// Receive ACKs for 0.5 seconds and returns the latest ACK received
void getAcks(uint16_t finalAck, int sockfd, sockaddr_in *si_other, socklen_t len) {
	chrono::milliseconds timer(TIMEOUT);
	keepGettingAcks = true;
	future<void> promise = async(launch::async, getAcksHelper, finalAck, sockfd, si_other, len);
	promise.wait_for(timer); // run for 0.5s
	keepGettingAcks = false;
}

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
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		return 1;
	}


	sockaddr_in addr, other;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(stoi(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr("127.0.0.1");

	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if (bind(sockfd, (sockaddr*) &addr, sizeof(addr)) == -1) {
		perror("bind");
		return 2;
	}

	socklen_t other_length = sizeof(other);
	TcpMessage received;
	TcpMessage toSend;
	bool hasReceivedSyn = false;
	bool sendFile = false;
	uint16_t seqToSend = rand() % MAX_SEQ_NUM; //The first sync
	uint16_t ackToSend;
	lastAckRecvd = seqToSend;
	uint16_t clientRecvWindow; // Set this each time client sends packet
	ifstream wantedFile;
	int packsToSend;
	int pktSent = 0;
	string flagsToSend = "";
	for (int packetsSent = 0; true; packetsSent++) {
	    cout << "Waiting for something" << endl;
		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			if (hasReceivedSyn) { // client ACK not received; resend SYN-ACK
				toSend.sendto(sockfd, &other, other_length);
				cout << "Sending SYN-ACK (retransmit):" << endl;
				toSend.dump();
			}
			continue;
		}

		cout << "Packet arrived from" << inet_ntoa(addr.sin_addr)<< ": " << ntohs(other.sin_port) << endl;
		cout << "Received:" << endl;
		received.dump();

		if (!packetsSent) {
			// Set receive timeout of 0.5s
			// Only want to run this code once
			timeval recvTimeval;
			recvTimeval.tv_sec = 0;
			recvTimeval.tv_usec = TIMEOUT * 1000;
			setSocketTimeout(sockfd, recvTimeval);
		}


		//Send SYN-ACK if client is trying to set up connection
	      
		switch (received.flags) {
		case SYN_FLAG:
		    if (hasReceivedSyn) {
				cerr << "Multiple SYNs\n"; 
				break;
			}
		    hasReceivedSyn = true;
		    flagsToSend ="SA";
		   
		    break;
		
		case SYN_FLAG | ACK_FLAG:
		    cerr << "Both SYN and ACK were set by client\n";
		    //exit(1);
		    break;

		case ACK_FLAG:
		    if (!hasReceivedSyn) {
				cerr << "ACK before handshake\n"; 
				break;
			}
		    flagsToSend = "A";
		    //Time to start sending the file back
		    sendFile = true;
		    
		    break;
		    
		default:
		    cerr << "Incorrect flags set\n";
		    break;
		}

		// if we're done handshake and are ready to send the file now
		if (sendFile) {
			break;
		}	      

		ackToSend = incSeqNum(received.seqNum, 1);
		clientRecvWindow = received.recvWindow;
		toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
		toSend.sendto(sockfd, &other, other_length);
		cout << "Handshake: sending packet\n";
		toSend.dump();
	}

	//return 0;
	
	char filebuf[DATA_SIZE];
	wantedFile.open(filename);
	if (!wantedFile) {
		perror("fstream open");
		exit(1);
	}

	off_t bodyLength = 0;
	struct stat st; // of course C names a class and function the same thing...
	if (stat(filename.c_str(), &st) == -1) {
		perror("stat");
	} 
	bodyLength = st.st_size;
	packsToSend = 1 + ((bodyLength - 1) / DATA_SIZE);

	// if the client sent data, increment ack by that number
	size_t recvLength = received.data.length();
	ackToSend = incSeqNum(received.seqNum, recvLength);
	// increment our sequence number by 1
	seqToSend = incSeqNum(seqToSend, 1); 

	unordered_map<uint16_t, TcpMessage> packetsInWindow; // seqNum => TcpMessage

	for (int filepkts = 0; filepkts < packsToSend; filepkts++, pktSent++) {

		memset(filebuf, 0, DATA_SIZE);

		//Read DATA_SIZE bytes normally, otherwise read the exact amount needed for the last packet
		int bytesToGet = ((filepkts == (packsToSend-1)) && (bodyLength % DATA_SIZE != 0))  ? (bodyLength % DATA_SIZE) : DATA_SIZE;
		wantedFile.read(filebuf, bytesToGet);
		toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, "A");
		toSend.data = string(filebuf, bytesToGet);

		// store packet in case retransmission needed
		packetsInWindow[toSend.seqNum] = toSend;

		cout << "sending packet " << filepkts << " of file: "<< filename << endl;
		toSend.dump();
		toSend.sendto(sockfd, &other, other_length);

		// Increase sequence number to send next time by number of bytes sent
		seqToSend = incSeqNum(seqToSend, bytesToGet);
	}

	uint16_t lastAckExpected = seqToSend;

	// receive ACKs and retransmit until all packets ACKed
	while (true) {
		getAcks(lastAckExpected, sockfd, &other, other_length);
		// all packets acked
		if (lastAckRecvd == lastAckExpected) {
			break;
		}
		// retransmit
		if (packetsInWindow.count(lastAckRecvd) == 0) {
			cerr << "packet with sequence number " << lastAckRecvd
				 << " not in packetsInWindow" << '\n';
		}
		else {
			cout << "Retransmitting: " << lastAckRecvd << endl;
			//packetsInWindow[lastAckRecvd].dump();
			packetsInWindow[lastAckRecvd].sendto(sockfd, &other, other_length);
		}
	}
	cout << "out of loop\n";

	/*
	for (int filepkts = 0; filepkts < packsToSend; filepkts++) {
		received.recvfrom(sockfd, &other, other_length);
		// receive ack for data
		cout << "Received ack for data:\n";
		received.dump();
		switch (received.flags) {
			case ACK_FLAG:
				// success
				break;
			default:
				cerr << "ACK not received!";
		}
	}
	*/

	bool hasReceivedFin = false;
	bool hasReceivedFinAck = false;
	int timedWaitTimer = 2 * MAX_SEG_LIFETIME; // milliseconds to wait before closing
	while (timedWaitTimer > 0) {

		/* send FIN if no FIN-ACK received yet */
		if (!hasReceivedFinAck) {
			flagsToSend = "F";
			// Sequence number shouldn't change since we already increased by total data size
			ackToSend = 0; // ACK is invalid this packet
			toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
			toSend.sendto(sockfd, &other, other_length);
			cout << "Sending FIN\n";
			toSend.dump();
		}

		/* timed wait timer counts down after FIN received */
		if (hasReceivedFin) {
			timedWaitTimer -= TIMEOUT; // wait TIMEOUT msec each loop
		}

		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			cout << "Timeout while waiting for FIN-ACK/FIN\n";
			continue;
		}
		switch(received.flags) {
			// FIN-ACK
			case FIN_FLAG | ACK_FLAG:
				// TODO: success
				cout << "Received FIN-ACK\n";
				hasReceivedFinAck = true;
				break;

			// FIN
			case FIN_FLAG:
				cout << "Received FIN\n";
				hasReceivedFin = true;

				flagsToSend = "A";// this is "FIN-ACK" but without FIN flag
				seqToSend = incSeqNum(seqToSend, 1);// increase sequence number by 1
				ackToSend = incSeqNum(received.seqNum, 1); // increase ack by 1
				toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
				toSend.sendto(sockfd, &other, other_length);
				cout << "Sending ACK of FIN\n";
				toSend.dump();
				cout << "Server received FIN; starting timed wait..." << endl;
				break;

			default:
				cerr << "Received packet wasn't FIN-ACK or FIN!\n";
				//exit(1);
		}
		received.dump();
	}

	//hasReceivedFin = false;
	//while (!hasReceivedFin) {
		/* Should receive FIN from client */
	/*
		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			cout << "Timeout while waiting for FIN\n";
			continue;
		}
		switch(received.flags) {
			case FIN_FLAG:
				// TODO: success
				cout << "Received FIN\n";
				hasReceivedFin = true;
				break;
			default:
				cerr << "FIN wasn't received!\n";
				//exit(1);
		}
		received.dump();
	}
	*/

	cout << "Shouldn't receive anything else from client now\n";
	//received.recvfrom(sockfd, &other, other_length);
	/* TODO: shouldn't receive anything from client so deal with cases where client sends stuff */
	
	close(sockfd);
	return 0;
}
