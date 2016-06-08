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
#include <unordered_map>

#include "Utils.h"
#include "TcpMessage.h"

using namespace std;
 

void printRecv(uint16_t seq) {
	cout << "Receiving packet " << seq << '\n';
}

void printSend(string pktType, uint16_t ack, bool isRetransmit) {
	cout << "Sending packet";
	if (pktType != "SYN") {
   		cout << " " << ack;
	}
	if (isRetransmit) {
		cout << " Retransmission";
	}
	if (pktType != "ACK") {
		// SYN or FIN
		cout << " " << pktType;
	}
	cout << '\n';
}

#define OUTPUT_FILE "received.data" // the file received and saved by the client
 
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
		// cerr << "getaddrinfo: " << gai_strerror(status) << '\n';
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
		// cerr << "IP address not found for " << ip << endl;
		exit(3);
	}
	ip = ipstr;
	// cerr << "IP: " << ip << "\n";

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
		// keep this // cerr output for now, remove it later
		// cerr << "sending SYN:" << endl;
		packetToSend.dump();
		packetToSend.sendto(sockfd, &si_server, serverLen);

		/* receive SYN-ACK */
		int r = packetReceived.recvfrom(sockfd, &si_server, serverLen);
		if (r == RECV_TIMEOUT) {
			continue; // resend SYN
		}
		else {
			printRecv(packetReceived.seqNum);
			// cerr << "receiving SYN-ACK:" << endl;
			packetReceived.dump();
			if (!packetReceived.getFlag('a') || !packetReceived.getFlag('s')) {
				// error: server responded, but without syn-ack
				// cerr << "Server responded, but without syn-ack!\n";
				//exit(1);
			}
			if (packetReceived.ackNum != incSeqNum(seqToSend, 1)) {
				// cerr << "SYN-ACK has wrong ack number; drop packet" << endl;
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
	unordered_map<uint16_t, TcpMessage> outOfOrderPkts; // seqNum -> TcpMessage
	// first data packet's sequence number we should receive
	uint16_t nextInOrderSeq = ackToSend;
	uint16_t lastAckSent = BAD_SEQ_NUM;
	bool handshakeAckResend = false;

	while (true) {
		/* send handshake ACK */
		if (!handshakeComplete) {
			packetToSend = TcpMessage(handshakeSeq, handshakeAck, recvWindowToSend, "A");
			printSend("ACK", handshakeAck, handshakeAckResend);
			lastAckSent = handshakeAck;
			// cerr << "sending ACK:" << endl;
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
		// cerr << "receiving data:" << endl;
		packetReceived.dump();

		// FIN received
		if (packetReceived.getFlag('F')) {
			// cerr << "OoO map size at FIN: " << outOfOrderPkts.size() << '\n';
			printRecv(packetReceived.seqNum);
			break;
		}
		printRecv(packetReceived.seqNum);

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
			// cerr << "Received pkt not w/in window!\n";
		}
		if (!shouldDropPkt) {
			if (seqReceived == nextInOrderSeq) { 
				// cerr << "Received in order packet!\n";
				// save data to file
				const char *data = packetReceived.data.c_str();
				outFile.write(data, dataSize);

				// see if we received an OoO pkt earlier w/ this seq #
				if (outOfOrderPkts.count(seqReceived)) {
					// delete it from map
					outOfOrderPkts.erase(seqReceived);
				}
				// update the next expected sequence number, increase by data size we got
				nextInOrderSeq = incSeqNum(nextInOrderSeq, dataSize);
				// loop through saved OoO packets to see if they're next in order
				while (outOfOrderPkts.count(nextInOrderSeq)) {
					// cerr << "Writing OoO pkt w/ seq # " << nextInOrderSeq << " from map\n";
					TcpMessage poppedMsg = outOfOrderPkts[nextInOrderSeq];
					const char *poppedData = poppedMsg.data.c_str();
					streamsize poppedDataSize = poppedMsg.data.size();
					outFile.write(poppedData, poppedDataSize);
					outOfOrderPkts.erase(nextInOrderSeq);
					nextInOrderSeq = incSeqNum(nextInOrderSeq, poppedDataSize);
				}
			}
			else {
				// out of order, store it for later
				// cerr << "Received out of order packet! Writing to map\n";
				outOfOrderPkts[seqReceived] = packetReceived;
			}
		}
		ackToSend = nextInOrderSeq; 
		dataAck = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "A");
		printSend("ACK", ackToSend, ackToSend == lastAckSent);
		// cerr << "sending ACK:" << endl;
		dataAck.dump();
		dataAck.sendto(sockfd, &si_server, serverLen);
		lastAckSent = ackToSend;
	}

 

	/* Send FIN-ACK for the FIN received earlier */
	bool hasSentFinAck = false;
	bool hasSentFin = false;
	bool hasReceivedAck = false; /* receive packets until a FIN-ACK is received from server */
	// seq # stays same b/c no payload
	ackToSend = incSeqNum(packetReceived.seqNum, 1);
	packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
	packetToSend.sendto(sockfd, &si_server, serverLen);
	printSend("FIN", ackToSend, hasSentFinAck);
	// cerr << "Sending FIN-ACK to server\n";
	packetToSend.dump();

	while (!hasReceivedAck) {

		/* Send FIN; seq # stays same b/c no payload */
		packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "F");
		packetToSend.sendto(sockfd, &si_server, serverLen);
		printSend("FIN", ackToSend, hasSentFin);
		hasSentFin = true; // so "retransmission" prints the second time
		// cerr << "Sending FIN to server\n";
		packetToSend.dump();

		int r = packetReceived.recvfrom(sockfd, &si_server, serverLen);
		if (r == RECV_TIMEOUT) {
			continue;
		}
		printRecv(packetReceived.seqNum);
		switch(packetReceived.flags) {
			// server resends FIN
			case FIN_FLAG:
				/* Send FIN-ACK */
				printSend("FIN", ackToSend, true);
				// cerr << "Received FIN\n";
				ackToSend = incSeqNum(packetReceived.seqNum, 1);
				packetToSend = TcpMessage(seqToSend, ackToSend, recvWindowToSend, "FA");
				packetToSend.sendto(sockfd, &si_server, serverLen);
				// cerr << "Sending FIN-ACK to server\n";
				packetToSend.dump();
				break;

			// get FIN-ACK; can exit now
			case ACK_FLAG:
				// cerr << "Received ACK of FIN! Closing connection.\n";
				hasReceivedAck = true;
				break;

			default:
				// cerr << "Received packet wasn't FIN-ACK or FIN!\n";
				//exit(1);
				break;

		}
	}



	close(sockfd);
	outFile.close();
	return 0;
}
