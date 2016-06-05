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

void printRecv(string pktType, uint16_t ack) {
	cout << "Receiving " << pktType << " packet " << ack << '\n';
}

void printSend(string pktType, uint16_t seq, int cwnd, int ssThresh, bool isRetransmit) {
	cout << "Sending " << pktType << " packet " << seq << " " << cwnd << " " << ssThresh;
	if (isRetransmit) {
		cout << " Retransmission";
	}
	cout << '\n';
}

bool keepGettingAcks = true;
uint16_t lastAckRecvd;
uint16_t clientRecvWindow; // Set this each time client sends packet
uint16_t rwndBot;
uint16_t rwndTop;
bool getAckTimedOut = false;

void getAcksHelper(int sockfd, sockaddr_in *si_other, socklen_t len) {
	TcpMessage ack;
	while (keepGettingAcks) {
		if (ack.recvfrom(sockfd, si_other, len) == RECV_SUCCESS) {
			getAckTimedOut = false;
			printRecv("ACK", ack.ackNum);
			ack.dump();
			lastAckRecvd = ack.ackNum;
			clientRecvWindow = ack.recvWindow;
			uint16_t start = incSeqNum(rwndBot, 1);
			uint16_t end = incSeqNum(rwndTop, DATA_SIZE);
			if (inWindow(lastAckRecvd, start, end)) {
				return; // new ACK received for something in receiver window
			}
		}
		else {
			cerr << "no ACK received" << endl;
		}
	}
}

// Receive ACKs for 0.5 seconds and returns the latest ACK received
void getAcks(int sockfd, sockaddr_in *si_other, socklen_t len) {
	chrono::milliseconds timer(TIMEOUT);
	keepGettingAcks = true;
	getAckTimedOut = true;;
	future<void> promise = async(launch::async, getAcksHelper, sockfd, si_other, len);
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
	ifstream wantedFile;
	string flagsToSend = "";
	for (int packetsSent = 0; true; packetsSent++) {
	    cerr << "Waiting for something" << endl;
		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			if (hasReceivedSyn) { // client ACK not received; resend SYN-ACK
				printSend("SYN-ACK", toSend.seqNum, 1, INIT_RECV_WINDOW, true);
				toSend.sendto(sockfd, &other, other_length);
				cerr << "Sending SYN-ACK (retransmit):" << endl;
				toSend.dump();
			}
			continue;
		}

		cerr << "Packet arrived from" << inet_ntoa(addr.sin_addr)<< ": " << ntohs(other.sin_port) << endl;
		cerr << "Received:" << endl;
		received.dump();

		clientRecvWindow = received.recvWindow;

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
			printRecv("SYN", received.ackNum);
		    if (hasReceivedSyn) {
				cerr << "Got another SYN, sending SYN-ACK\n";
			}
		    flagsToSend ="SA";
			printSend("SYN-ACK", seqToSend, 1, INIT_RECV_WINDOW, hasReceivedSyn);
		    hasReceivedSyn = true;

		    break;

		case SYN_FLAG | ACK_FLAG:
		    cerr << "Both SYN and ACK were set by client\n";
			// TODO: should we deal with this case?
		    //exit(1);
		    break;

		case ACK_FLAG:
			printRecv("ACK", received.ackNum);
		    if (!hasReceivedSyn) {
				// TODO: think about this case more
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
		toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
		toSend.sendto(sockfd, &other, other_length);
		cerr << "Handshake: sending packet\n";
		toSend.dump();
	}

	//return 0;

	char filebuf[DATA_SIZE];
	wantedFile.open(filename);
	if (!wantedFile) {
		perror("fstream open");
		exit(1);
	}

	/*
	off_t bodyLength = 0;
	struct stat st; // of course C names a class and function the same thing...
	if (stat(filename.c_str(), &st) == -1) {
		perror("stat");
	}
	bodyLength = st.st_size;
	*/

	// if the client sent data, increment ack by that number
	size_t recvLength = received.data.length();
	ackToSend = incSeqNum(received.seqNum, recvLength);
	// increment our sequence number by 1
	seqToSend = incSeqNum(seqToSend, 1);

	unordered_map<uint16_t, TcpMessage> packetsInWindow; // seqNum => TcpMessage
	double cwnd = DATA_SIZE;
	uint16_t cwndBot = seqToSend;
	uint16_t cwndTop = seqToSend + cwnd;
	bool useSlowStart = true;
	uint16_t ssThresh = INIT_RECV_WINDOW;
	int filepkts = 0;

	uint16_t lastAckExpected = 54321; // initialize to some impossible value
	lastAckRecvd = seqToSend;



	/* data */

	while(true) {

		// update receiver/congestion windows
		rwndBot = lastAckRecvd;
		rwndTop = incSeqNum(rwndBot, clientRecvWindow);
		cwndBot = lastAckRecvd;
		cwndTop = incSeqNum(cwndBot, cwnd);

		// remove old packets that are outside receiver window
		auto it = packetsInWindow.begin(); // iterator
		while (it != packetsInWindow.end()) {
			auto curr = it;
			it++;
			uint16_t currSeq = curr->second.seqNum;
			if (!inWindow(currSeq, rwndBot, rwndTop)) {
				packetsInWindow.erase(curr);
			}
		}

		// create new packets
		uint16_t seqToSendEnd = incSeqNum(seqToSend, DATA_SIZE);
		while (inWindow(seqToSendEnd, rwndBot, rwndTop) &&
			   inWindow(seqToSendEnd, cwndBot, cwndTop) &&
			   lastAckExpected == 54321) {

			memset(filebuf, 0, DATA_SIZE);
			wantedFile.read(filebuf, DATA_SIZE);
			streamsize dataSize = wantedFile.gcount();
			toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, "A");
			toSend.data = string(filebuf, dataSize);

			// no more data; this packet's seq should be the last ACK
			if (dataSize == 0) {
				lastAckExpected = seqToSend;
				break;
			}

			// if already in map, we're retransmitting
			bool isRetransmit = packetsInWindow.count(toSend.seqNum);
			// store packet in case retransmission needed
			packetsInWindow[toSend.seqNum] = toSend;

			printSend("data", toSend.seqNum, cwnd, ssThresh, isRetransmit);
			cerr << "sending packet " << filepkts << " of file: " << endl;
			toSend.dump();
			toSend.sendto(sockfd, &other, other_length);
			filepkts++;

			seqToSend = incSeqNum(seqToSend, dataSize);
			seqToSendEnd = incSeqNum(seqToSend, DATA_SIZE);
		}

	    getAcks(sockfd, &other, other_length);

		// no ACK received
		if (getAckTimedOut) {
			// retransmit oldest data packet
			if (packetsInWindow.count(lastAckRecvd) == 0) {
				cerr << "!!!!! packet not in window: " << lastAckRecvd << endl;
				cerr << "!!!!! should never reach this" << endl;
				continue;
			}
			cerr << "retransmitting" << endl;
			packetsInWindow[lastAckRecvd].dump();
			packetsInWindow[lastAckRecvd].sendto(sockfd, &other, other_length);

			// update congestion control
			ssThresh = cwnd / 2;
			cwnd = DATA_SIZE;
			useSlowStart = true;
		}
		// got an ACK
		else {
			// check if we are done
			// this can only be true after the last packet is sent; otherwise,
			// lastAckExpected has a impossible value
			if (lastAckRecvd == lastAckExpected) {
				break;
			}

			// update congestion control
			if (useSlowStart) {
				cwnd = cwnd + DATA_SIZE;
			}
			else {
				cwnd = cwnd + DATA_SIZE/cwnd;
			}
		}
	}


	cerr << "out of loop\n";


	/* FIN */

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
			cerr << "Sending FIN\n";
			toSend.dump();
		}

		/* timed wait timer counts down after FIN received */
		if (hasReceivedFin) {
			timedWaitTimer -= TIMEOUT; // wait TIMEOUT msec each loop
		}

		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			cerr << "Timeout while waiting for FIN-ACK/FIN\n";
			continue;
		}
		clientRecvWindow = received.recvWindow;

		switch(received.flags) {
			// FIN-ACK
			case FIN_FLAG | ACK_FLAG:
				// TODO: success
				printRecv("FIN-ACK", received.ackNum);
				cerr << "Received FIN-ACK\n";
				hasReceivedFinAck = true;
				break;

			// FIN
			case FIN_FLAG:
				hasReceivedFinAck = true; // assume that client got a FIN
				hasReceivedFin = true;
				printRecv("FIN", received.ackNum);
				cerr << "Received FIN\n";

				flagsToSend = "A";// this is "FIN-ACK" but without FIN flag
				seqToSend = incSeqNum(seqToSend, 1);// increase sequence number by 1
				ackToSend = incSeqNum(received.seqNum, 1); // increase ack by 1
				toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
				toSend.sendto(sockfd, &other, other_length);
				cerr << "Sending ACK of FIN\n";
				toSend.dump();
				cerr << "Server received FIN; starting timed wait..." << endl;
				break;

			default:
				cerr << "Received packet wasn't FIN-ACK or FIN!\n";
				//exit(1);
		}
		received.dump();
	}

	cerr << "Shouldn't receive anything else from client now\n";

	close(sockfd);
	return 0;
}
