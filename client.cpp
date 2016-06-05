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
#include <deque>

#include "Utils.h"
#include "TcpMessage.h"

using namespace std;
 

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

void printRecv(string pktType, uint16_t seq) {
	cout << "Receiving " << pktType << " packet " << seq << '\n';
}

void printSend(string pktType, uint16_t ack, bool isRetransmit) {
	cout << "Sending " << pktType << " packet " << ack;
	if (isRetransmit) {
		cout << " Retransmission";
	}
	cout << '\n';
}

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
	cerr << "IP: " << ip << "\n";

	srand (time(NULL)); //Used to generate random ISN
 
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
		exit(1);
    }

	// Set receive timeout of 0.5 s
	timeval recvTimeval;
	recvTimeval.tv_sec = 0;
	recvTimeval.tv_usec = TIMEOUT * 1000;
	setSocketTimeout(sockfd, recvTimeval);
 
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
	uint16_t synAckSeq;


	/* handshake */
	bool hasResentSyn = false;
	while (true) {
		/* send SYN */
		packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "S");
		printSend("SYN", seqToSend, hasResentSyn);
		hasResentSyn = true; // set this to true so "retransmit" will print the 2nd time
		// keep this cerr output for now, remove it later
		cerr << "sending SYN:" << endl;
		packetToSend.dump();
		packetToSend.sendto(sockfd, &si_server, serverLen);

		/* receive SYN-ACK */
		int r = packetReceived.recvfrom(sockfd, &si_server, serverLen);
		if (r == RECV_TIMEOUT) {
			continue; // resend SYN
		}
		else {
			printRecv("SYN-ACK", packetReceived.seqNum);
			cerr << "receiving SYN-ACK:" << endl;
			packetReceived.dump();
			if (!packetReceived.getFlag('a') || !packetReceived.getFlag('s')) {
				// error: server responded, but without syn-ack
				// TODO
				cerr << "Server responded, but without syn-ack!\n";
				//exit(1);
			}
			if (packetReceived.ackNum != incSeqNum(seqToSend, 1)) {
				cerr << "SYN-ACK has wrong ack number; drop packet" << endl;
				continue;
			}
			synAckSeq = packetReceived.seqNum;
			break;
		}
	}

	// seq/ack for client's ACK for handshake
	seqToSend = incSeqNum(seqToSend, 1);// increase sequence number by 1
	uint16_t handshakeSeq = seqToSend;
	ackToSend = incSeqNum(packetReceived.seqNum, 1);
	uint16_t handshakeAck = ackToSend;

	//return 0;

	/* receive data */

	ofstream outFile(OUTPUT_FILE);
	bool handshakeComplete = false;
	TcpMessage dataAck;

	// store the out-of-order packets in case we get in order packets
	deque<TcpMessage> outOfOrderPkts;
	// first data packet's sequence number we should receive
	uint16_t nextInOrderSeq = ackToSend;
	bool handshakeAckResend = false;

	while (true) {
		/* send handshake ACK */
		if (!handshakeComplete) {
			packetToSend = TcpMessage(handshakeSeq, handshakeAck, recvWindowToSend, "A");
			printSend("ACK", seqToSend, handshakeAckResend);
			cerr << "sending ACK:" << endl;
			packetToSend.dump();
			packetToSend.sendto(sockfd, &si_server, serverLen);
			handshakeAckResend = true;
		}

		/* receive data packet */
		int r = packetReceived.recvfrom(sockfd, &si_server, serverLen);
		// if timeout, try to recvfrom again
		if (r == RECV_TIMEOUT) {
			continue;
		}
		// if duplicate SYN-ACK, resend handshake ACK
		if (packetReceived.seqNum == synAckSeq) {
			continue;
		}
		handshakeComplete = true; // done with handshake if this is a data pkt

		// else, r == RECV_SUCCESS
		cerr << "receiving data:" << endl;
		packetReceived.dump();

		// FIN received
		if (packetReceived.getFlag('F')) {
			printRecv("FIN", packetReceived.seqNum);
			break;
		}
		printRecv("data", packetReceived.seqNum);

		streamsize dataSize = packetReceived.data.size();
		uint16_t seqReceived = packetReceived.seqNum;
		bool shouldDropPkt = false;
		// check if packet's within window boundary; if not, drop packet
		uint16_t windowMaxSeq = incSeqNum(nextInOrderSeq, recvWindowToSend);
		if (windowMaxSeq > nextInOrderSeq) {
			// no wrap around
			shouldDropPkt = seqReceived < nextInOrderSeq || seqReceived > windowMaxSeq;
		}
		else {
			// wrap around
			shouldDropPkt = seqReceived > windowMaxSeq && seqReceived < nextInOrderSeq;
		}
		if (shouldDropPkt) {
			cerr << "Received pkt not w/in window!\n";
		}
		if (!shouldDropPkt) {
			if (seqReceived == nextInOrderSeq) { 
				cerr << "Received in order packet!\n";
				// save data to file
				const char *data = packetReceived.data.c_str();
				outFile.write(data, dataSize);
				// update the next expected sequence number, increase by data size we got
				nextInOrderSeq = incSeqNum(nextInOrderSeq, dataSize);
				streamsize nextDataSize = dataSize;
				// loop through saved OoO packets to see if they're next in order
				while (outOfOrderPkts.size()) {
					uint16_t nextSeq = outOfOrderPkts[0].seqNum;
					// first packet in deque is the next one! pop it off and keep going
					if (nextSeq == nextInOrderSeq) {
						cerr << "Popping OoO packet w/ seq num " << nextSeq << '\n';
						// get the OoO packet's data and data size, write to disk
						data = outOfOrderPkts[0].data.c_str();
						nextDataSize = outOfOrderPkts[0].data.size();
						outFile.write(data, nextDataSize);
						// update next expected sequence number
						nextInOrderSeq = incSeqNum(nextInOrderSeq, nextDataSize);
						outOfOrderPkts.pop_front();
					}
					else {
						break;
					}
				}
			}
			else {
				// out of order, store it for later
				cerr << "Received out of order packet!\n";
				unsigned currVecSize = outOfOrderPkts.size();
				// no OoO packets yet
				if (!currVecSize) {
					cerr << "OoO list was empty, this is only OoO packet\n";
					outOfOrderPkts.push_back(packetReceived);
				}
				// received packet belongs after all other OoO packets
				// if its seqNum >= the last packet's seqNum + data size
				// do this first in case the seq #'s wrapped around
				else if (seqReceived >= incSeqNum(outOfOrderPkts.back().seqNum, outOfOrderPkts.back().data.size())) {
					outOfOrderPkts.push_back(packetReceived);
					cerr << "OoO packet pushed to end of list\n";
				}
				// received packet belongs before all other OoO packets
				// if its seqNum < first packet's seqNum
				else if (seqReceived < outOfOrderPkts[0].seqNum) {
					outOfOrderPkts.push_front(packetReceived);
					cerr << "OoO packet inserted at front of list\n";
				}
				// else, received packet belongs in b/w two of the OoO packets
				else {
					for (unsigned i = 0; i < currVecSize; i++) {
						uint16_t currSeq = outOfOrderPkts[i].seqNum;
						if (seqReceived == currSeq) {
							// already got this data packet, ignore it
							cerr << "OoO packet discarded: seqNum " << currSeq << '\n';
							shouldDropPkt = true;
							break;
						}
					}
					if (!shouldDropPkt) {
						for (unsigned i = 1; i < currVecSize; i++) {
							uint16_t currSeq = outOfOrderPkts[i].seqNum;
							// received packet's seqNum is >= the first packet's seqNum + data size and < second packet's seqNum
							// TODO: fix this for case where seq #'s wrap around
							if (seqReceived >= incSeqNum(outOfOrderPkts[i - 1].seqNum, outOfOrderPkts[i - 1].data.size()) && seqReceived < currSeq) {
								outOfOrderPkts.insert(outOfOrderPkts.begin() + i, packetReceived);
								cerr << "OoO packet inserted in middle at pos " << i << '\n';
								break;
							}
						}
					}
				}
			}
		}
		ackToSend = nextInOrderSeq; 
		dataAck = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "A");
		printSend("ACK", ackToSend, shouldDropPkt);
		cerr << "sending ACK:" << endl;
		dataAck.dump();
		dataAck.sendto(sockfd, &si_server, serverLen);
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

	/* Send FIN-ACK for the FIN received earlier */
	// seq # stays same b/c no payload
	ackToSend = incSeqNum(packetReceived.seqNum, 1);
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	printSend("FIN-ACK", ackToSend, false);
	cerr << "Sending FIN-ACK to server\n";
	packetToSend.dump();

	bool hasSentFin = false;
	bool hasReceivedAck = false; /* receive packets until a FIN-ACK is received from server */
	while (!hasReceivedAck) {

		/* Send FIN; seq # stays same b/c no payload */
		packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "F");
		packetToSend.sendto(sockfd, &si_server, serverLen);
		printSend("FIN", ackToSend, hasSentFin);
		hasSentFin = true; // so "retransmission" prints the second time
		cerr << "Sending FIN to server\n";
		packetToSend.dump();

		int r = packetReceived.recvfrom(sockfd, &si_server, serverLen);
		if (r == RECV_TIMEOUT) {
			continue;
		}
		switch(packetReceived.flags) {
			// server resends FIN
			case FIN_FLAG:
				/* Send FIN-ACK */
				cerr << "Received FIN\n";
				ackToSend = incSeqNum(packetReceived.seqNum, 1);
				packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
				packetToSend.sendto(sockfd, &si_server, serverLen);
				cerr << "Sending FIN-ACK to server\n";
				packetToSend.dump();
				break;

			// get FIN-ACK; can exit now
			case ACK_FLAG:
				cerr << "Received ACK of FIN! Closing connection.\n";
				hasReceivedAck = true;
				break;

			default:
				cerr << "Received packet wasn't FIN-ACK or FIN!\n";
				//exit(1);

		}
	}


	/* // Send FIN-ACK; seq # stays same b/c no payload
	ackToSend = incSeqNum(packetReceived.seqNum, 1);
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	cerr << "Sending FIN-ACK to server\n";
	packetToSend.dump();
	
	// Send FIN; seq # stays same b/c no payload //
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "F");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	cerr << "Sending FIN to server\n";
	packetToSend.dump();

    // Receive ACK of FIN without the FIN flag from server
	packetReceived.recvfrom(sockfd, &si_server, serverLen);
	switch (packetReceived.flags) {
		case ACK_FLAG:
			//TODO: success
			cerr << "Received ACK of FIN from server\n";
			break;
		default:
			cerr << "ACK of FIN wasn't received from server!\n";
			exit(1);
	}
	packetReceived.dump();*/

	close(sockfd);
	outFile.close();
	return 0;
}
