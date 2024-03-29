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

void printRecv(uint16_t ack) {
	cout << "Receiving packet " << ack << '\n';
}

void printSend(string pktType, uint16_t seq, int cwnd, int ssThresh, bool isRetransmit) {
	cout << "Sending packet " << seq << " " << cwnd << " " << ssThresh;
	if (isRetransmit) {
		cout << " Retransmission";
	}
	if (pktType != "data") {
		// SYN or FIN
		cout << " " << pktType;
	}
	cout << '\n';
}

bool keepGettingAcks = true;
uint16_t lastAckRecvd;
uint16_t clientRecvWindow; // Set this each time client sends packet
uint16_t rwndBot;
uint16_t rwndTop;
bool getAckTimedOut = false;
unordered_map<uint16_t, TcpMessage> packetsInWindow; // seqNum => TcpMessage
unordered_map<uint16_t, timeval> timestamps; // seqNum => timestamp
uint16_t timedOutSeq;
uint16_t acksInARow = 0;
uint16_t previousAck = BAD_SEQ_NUM;


int getAcks(int sockfd, sockaddr_in *si_other, socklen_t len) {
	// find the oldest ACK we are waiting for
	uint16_t oldestSeq = BAD_SEQ_NUM;
	timeval oldestTimeval = now();
	for (auto i : packetsInWindow) {
		uint16_t currSeq = i.first;
		// this packet is older
		if (timeval_cmp(timestamps[currSeq], oldestTimeval)) {
			oldestSeq = currSeq;
			oldestTimeval = timestamps[currSeq];
		}
	}
	// cerr << "oldest seq = " << oldestSeq << endl;

	// if this packet already timed out, just return
	timeval remaining = timeRemaining(oldestTimeval);
	if (remaining.tv_sec == 0 && remaining.tv_usec == 0) {
		timedOutSeq = oldestSeq;
		return RECV_TIMED_OUT_ALREADY;
	}

	// try to recv until this packet times out
	setSocketTimeout(sockfd, timeRemaining(oldestTimeval));

	TcpMessage ack;
	int r = ack.recvfrom(sockfd, si_other, len);
	if (r == RECV_SUCCESS) {
		printRecv(ack.ackNum);
		ack.dump();
		lastAckRecvd = ack.ackNum;
		clientRecvWindow = ack.recvWindow;
		return RECV_SUCCESS;
	}
	else { // RECV_TIMEOUT
		timedOutSeq = oldestSeq;
		return RECV_TIMEOUT;
	}
}



int main(int argc, char **argv) {
	if (argc != 3) {
		cerr << "usage: " << argv[0] << " PORT-NUMBER FILENAME" << '\n';
		return 1;
	}

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
	uint16_t ssThresh = MAX_SEQ_NUM;
	ifstream wantedFile;
	string flagsToSend = "";
	for (int packetsSent = 0; true; packetsSent++) {
	    // cerr << "Waiting for something" << endl;
		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			if (hasReceivedSyn) { // client ACK not received; resend SYN-ACK
				printSend("SYN", toSend.seqNum, DATA_SIZE, DATA_SIZE, true);
				toSend.sendto(sockfd, &other, other_length);
				// cerr << "Sending SYN-ACK (retransmit):" << endl;
				toSend.dump();
			}
			continue;
		}

		// cerr << "Packet arrived from" << inet_ntoa(addr.sin_addr)<< ": " << ntohs(other.sin_port) << endl;
		// cerr << "Received:" << endl;
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
			printRecv(received.ackNum);
		    if (hasReceivedSyn) {
				// cerr << "Got another SYN, sending SYN-ACK\n";
			}
		    flagsToSend ="SA";
			printSend("SYN", seqToSend, DATA_SIZE, DATA_SIZE, hasReceivedSyn);
		    hasReceivedSyn = true;

		    break;

		case SYN_FLAG | ACK_FLAG:
		    // cerr << "Both SYN and ACK were set by client\n";
		    //exit(1);
		    break;

		case ACK_FLAG:
			printRecv(received.ackNum);
		    if (!hasReceivedSyn) {
				// cerr << "ACK before handshake\n";
				break;
			}
		    flagsToSend = "A";
		    //Time to start sending the file back
		    sendFile = true;

		    break;

		default:
		    // cerr << "Incorrect flags set\n";
		    break;
		}

		// if we're done handshake and are ready to send the file now
		if (sendFile) {
			break;
		}

		ackToSend = incSeqNum(received.seqNum, 1);
		toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
		toSend.sendto(sockfd, &other, other_length);
		// cerr << "Handshake: sending packet\n";
		toSend.dump();
	}

	char filebuf[DATA_SIZE];
	wantedFile.open(filename);
	if (!wantedFile) {
		perror("fstream open");
		exit(1);
	}

	// if the client sent data, increment ack by that number
	size_t recvLength = received.data.length();
	ackToSend = incSeqNum(received.seqNum, recvLength);
	// increment our sequence number by 1
	seqToSend = incSeqNum(seqToSend, 1);

	double cwnd = DATA_SIZE;
	uint16_t cwndBot = seqToSend;
	uint16_t cwndTop = seqToSend + cwnd;
	bool useSlowStart = true;
	int filepkts = 0;

	uint16_t lastAckExpected = BAD_SEQ_NUM;
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
		// stop if next packet will be outside rwnd or cwnd
		while (inWindow(seqToSendEnd, cwndBot, cwndTop) &&
			   lastAckExpected == BAD_SEQ_NUM) {

			memset(filebuf, 0, DATA_SIZE);
			wantedFile.read(filebuf, DATA_SIZE);
			streamsize dataSize = wantedFile.gcount();
			toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, "A");
			toSend.data = string(filebuf, dataSize);

			// if already in map, we're retransmitting
			bool isRetransmit = packetsInWindow.count(toSend.seqNum);
			// store packet in case retransmission needed
			packetsInWindow[toSend.seqNum] = toSend;
			timestamps[toSend.seqNum] = now();

			seqToSend = incSeqNum(seqToSend, dataSize);
			seqToSendEnd = incSeqNum(seqToSend, DATA_SIZE);

			// send packet
			if (dataSize > 0) {
				printSend("data", toSend.seqNum, cwnd, ssThresh, isRetransmit);
				// cerr << "sending packet " << filepkts << " of file: " << endl;
				toSend.dump();
				toSend.sendto(sockfd, &other, other_length);
				filepkts++;
			}
			// no more data; this packet's seq should be the last ACK
			else {
				lastAckExpected = seqToSend;
				break;
			}

		}

	    int r = getAcks(sockfd, &other, other_length);

		// a old packet timed out; resend it
		if (r == RECV_TIMED_OUT_ALREADY) {
			// no packets in window
			if (timedOutSeq == BAD_SEQ_NUM) {
				// cerr << "!!!!!!!!!!!! no packets in window" << endl;
			}

			// cerr << "retransmit (timed out already)" << endl;
			packetsInWindow[timedOutSeq].dump();
			packetsInWindow[timedOutSeq].sendto(sockfd, &other, other_length);
			timestamps[timedOutSeq] = now();
		}

		// no ACK received
		else if (r == RECV_TIMEOUT) {
			// retransmit last ACKed packet
			if (packetsInWindow.count(lastAckRecvd) == 0) {
				// cerr << "!!!!! packet not in window: " << lastAckRecvd << endl;
				// cerr << "!!!!! should never reach this" << endl;
				for (auto i : packetsInWindow) {
					// cerr << i.first << " ";
				}
				// cerr << endl;
				if (packetsInWindow.size() == 0) {
					// cerr << "packetsInWindow is empty!" << endl;
				}
				continue;
			}
			// cerr << "retransmitting (timeout)" << endl;
			printSend("data", packetsInWindow[lastAckRecvd].seqNum, cwnd, ssThresh, true);
			packetsInWindow[lastAckRecvd].dump();
			packetsInWindow[lastAckRecvd].sendto(sockfd, &other, other_length);
			timestamps[lastAckRecvd] = now();

			// update congestion control
			ssThresh = cwnd / 2;
			if (ssThresh < DATA_SIZE) {
				ssThresh = DATA_SIZE;
			}
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

			// fast retransmit
			if (lastAckRecvd == previousAck) {
				acksInARow++; 
				if (acksInARow == 4) {
					// cerr << "retransmitting (fast retransmit)" << lastAckRecvd << endl;
					if (packetsInWindow.count(lastAckRecvd) == 0) {
						// cerr << "!!!!! packet not in window: " << lastAckRecvd << endl;
						// cerr << "!!!!! should never reach this" << endl;
						for (auto i : packetsInWindow) {
							// cerr << i.first << " ";
						}
						// cerr << endl;
						if (packetsInWindow.size() == 0) {
							// cerr << "packetsInWindow is empty!" << endl;
						}
						continue;
					}
					printSend("data", packetsInWindow[lastAckRecvd].seqNum, cwnd, ssThresh, true);
					packetsInWindow[lastAckRecvd].dump();
					packetsInWindow[lastAckRecvd].sendto(sockfd, &other, other_length);
					timestamps[lastAckRecvd] = now();
					acksInARow = 0;
					//fast recovery
					//should actually be same logic as timeout retransmit, just without setting useSlowStart
					ssThresh = (cwnd/2 < DATA_SIZE) ? DATA_SIZE : cwnd/2;
					cwnd = ssThresh > clientRecvWindow ? clientRecvWindow : ssThresh;
					useSlowStart = false;
					previousAck = lastAckRecvd;
					continue;
					
				}
			}
			previousAck = lastAckRecvd;
			
			// check if all packets in window were cumulatively ACKed
			if (lastAckRecvd == seqToSend) {
				// cerr << "!!!!!!!!! all packets ACKed" << endl;
			}

			// update congestion control
			if (useSlowStart) {
				cwnd = cwnd + DATA_SIZE;
			}
			else {
				cwnd = cwnd + DATA_SIZE * DATA_SIZE /cwnd;
			}
			if (cwnd > clientRecvWindow) {
				cwnd = clientRecvWindow;
			}
			if (cwnd >= ssThresh) {
				useSlowStart = false;
			}
		}
	}


	// cerr << "out of loop\n";


	/* FIN */

	// reset to 0.5s timeout
	timeval recvTimeval;
	recvTimeval.tv_sec = 0;
	recvTimeval.tv_usec = TIMEOUT * 1000;
	setSocketTimeout(sockfd, recvTimeval);

	bool hasReceivedFin = false;
	bool hasReceivedFinAck = false;
	bool hasSentFin = false;
	bool hasSentAckFin = false;
	int timedWaitTimer = 2 * MAX_SEG_LIFETIME; // milliseconds to wait before closing

	while (timedWaitTimer > 0) {

		/* send FIN if no FIN-ACK received yet */
		if (!hasReceivedFinAck) {
			flagsToSend = "F";
			// Sequence number shouldn't change since we already increased by total data size
			ackToSend = 0; // ACK is invalid this packet
			toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
			toSend.sendto(sockfd, &other, other_length);
			printSend("FIN", seqToSend, DATA_SIZE, ssThresh, hasSentFin);
			// cerr << "Sending FIN\n";
			toSend.dump();
			hasSentFin = true;
		}

		/* timed wait timer counts down after FIN received */
		if (hasReceivedFin) {
			timedWaitTimer -= TIMEOUT; // wait TIMEOUT msec each loop
		}

		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			// cerr << "Timeout while waiting for FIN-ACK/FIN\n";
			continue;
		}
		clientRecvWindow = received.recvWindow;

		switch(received.flags) {
			// FIN-ACK
			case FIN_FLAG | ACK_FLAG:
				printRecv(received.ackNum);
				// cerr << "Received FIN-ACK\n";
				hasReceivedFinAck = true;
				break;

			// FIN
			case FIN_FLAG:
				printRecv(received.ackNum);
				// cerr << "Received FIN\n";

				flagsToSend = "A";// this is "FIN-ACK" but without FIN flag
				seqToSend = incSeqNum(seqToSend, 1);// increase sequence number by 1
				ackToSend = incSeqNum(received.seqNum, 1); // increase ack by 1
				toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, flagsToSend);
				toSend.sendto(sockfd, &other, other_length);
				printSend("data", seqToSend, DATA_SIZE, ssThresh, hasSentAckFin);
				// cerr << "Sending ACK of FIN\n";
				toSend.dump();
				// cerr << "Server received FIN; starting timed wait..." << endl;
				hasReceivedFinAck = true; // assume that client got a FIN
				hasReceivedFin = true;
				hasSentAckFin = true;
				break;

			default:
				// cerr << "Received packet wasn't FIN-ACK or FIN!\n";
				//exit(1);
				break;
		}
		received.dump();
	}

	// cerr << "Shouldn't receive anything else from client now\n";

	close(sockfd);
	return 0;
}
