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

void printSend(string pktType, uint16_t seq, int cwndPkts, int ssThresh, bool isRetransmit) {
	cout << "Sending " << pktType << " packet " << seq << " " << cwndPkts * DATA_SIZE << " " << ssThresh;
	if (isRetransmit) {
		cout << " Retransmission";
	}
	cout << '\n';
}

bool keepGettingAcks = true;
uint16_t lastAckRecvd;
uint16_t clientRecvWindow; // Set this each time client sends packet
bool getAckTimedOut = false;

void getAcksHelper(uint16_t finalAck, int sockfd, sockaddr_in *si_other, socklen_t len, uint16_t unwantedAck) {
	TcpMessage ack;
	while (keepGettingAcks && lastAckRecvd != finalAck) {
		if (ack.recvfrom(sockfd, si_other, len) == RECV_SUCCESS) {
		    getAckTimedOut = false;
		    printRecv("ACK", ack.ackNum);
		    ack.dump();
		    lastAckRecvd = ack.ackNum;
		    clientRecvWindow = ack.recvWindow;
		    if(lastAckRecvd != unwantedAck)
			break;
		}
		else {
		    cerr << "no ACK received" << endl;
		}
	}	
}

// Receive ACKs for 0.5 seconds and returns the latest ACK received
void getAcks(uint16_t finalAck, int sockfd, sockaddr_in *si_other, socklen_t len, uint16_t unwantedAck) {
	chrono::milliseconds timer(TIMEOUT);
	keepGettingAcks = true;
	getAckTimedOut = true;;
	future<void> promise = async(launch::async, getAcksHelper, finalAck, sockfd, si_other, len, unwantedAck);
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
	lastAckRecvd = seqToSend + 1;
	ifstream wantedFile;
	int packsToSend;
	int pktSent = 0;
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
	//int congWindowSize = 1;
	int cwndBot = 0;//seqToSend;
	int cwndTop = 1;//seqToSend + DATA_SIZE;
	int cwndToSend = cwndBot;
	int congAvoidanceFlag=0;
	int congAvoidValue=cwndBot;
	int ssThresh = INIT_RECV_WINDOW;
	int filepkts = 0;
	//bool randomBool = true;
	int bytesToGet = 0;
	int windowStartSeq = seqToSend;
	int dynWindowStartSeq = windowStartSeq;//This one changes
	uint16_t lastAckExpected;
	uint16_t unwantedAck = seqToSend;//This is the ACK that would be send if the client gets packets out of order and does its cumulative ack, we check for this to see when we finally receive a correct ACK
	while(true){
	   
	    unwantedAck = lastAckRecvd;
	    dynWindowStartSeq = incSeqNum(windowStartSeq, DATA_SIZE * cwndBot);
	    for(filepkts =  cwndToSend; filepkts < cwndTop && filepkts < packsToSend; filepkts++, pktSent++){

		memset(filebuf, 0, DATA_SIZE);
		

		//Read DATA_SIZE bytes normally, otherwise read the exact amount needed for the last packet
		bytesToGet = ((filepkts == (packsToSend-1)) && (bodyLength % DATA_SIZE != 0))  ? (bodyLength % DATA_SIZE) : DATA_SIZE;
		wantedFile.read(filebuf, bytesToGet);
		toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, "A");
		toSend.data = string(filebuf, bytesToGet);
	    
		// if already in map, we're retransmitting
		bool isRetransmit = packetsInWindow.count(toSend.seqNum);
		// store packet in case retransmission needed
		packetsInWindow[toSend.seqNum] = toSend;

		printSend("data", toSend.seqNum, cwndTop - cwndBot, ssThresh, isRetransmit);
		cout << "sending packet " << filepkts << " of file: "<< filename << endl;
		toSend.dump();
		toSend.sendto(sockfd, &other, other_length);

		// Increase sequence number to send next time by number of bytes sent
		seqToSend = incSeqNum(seqToSend, bytesToGet);
	    }
	    
	    //The first number that we expect from the client
	    lastAckExpected = incSeqNum(dynWindowStartSeq, DATA_SIZE);

	    // receive ACKs and retransmit until all packets ACKed
	    getAcks(lastAckExpected, sockfd, &other, other_length, unwantedAck);
	    // We successfully received an ack, so we increase the window size by 1 and move it
	    if (getAckTimedOut){
		//No ACKS received so we have to retransmitt
		cout << "Retransmitting (timeout): " << lastAckRecvd << endl;
		ssThresh = (cwndTop-cwndBot+1)/2;
		cwndToSend = cwndBot;
		cwndTop = cwndBot+1;
		congAvoidanceFlag = 0;
		//seqToSend = (seqToSend + MAX_SEQ_NUM -  bytesToGet) % MAX_SEQ_NUM;
		seqToSend = lastAckRecvd;
		wantedFile.seekg(cwndBot * DATA_SIZE, ios::beg);
		continue;

	    }
	    if (lastAckRecvd == lastAckExpected && congAvoidanceFlag){
		cwndToSend = cwndTop; cwndBot++; cwndTop++;
		if (cwndBot == congAvoidValue){
		    cwndTop++; congAvoidValue = cwndTop +1;}
		
	    /*
	    else if (lastAckRecvd == lastAckExpected) {
		if(cwndTop-cwndBot < ssThresh){
		    cwndBot++;
		    cwndTop+=2;
		    unwantedAck = lastAckRecvd;
		    if(cwndTop - cwndBot >= ssThresh){
			congAvoidanceFlag=1;
			congAvoidValue = cwndTop+1;
		    }
		    continue;   
		}
		*/
	    } else if (lastAckRecvd >= lastAckExpected || (lastAckRecvd < lastAckExpected && lastAckRecvd < dynWindowStartSeq)){//TODO: use receive window to check if packet is in window
		cerr << "=========================================" << endl;
		cwndToSend = cwndTop;
		cwndBot+=(1 + (lastAckRecvd-lastAckExpected)/DATA_SIZE);
		cwndTop+=(2 + 2*(lastAckRecvd-lastAckExpected)/DATA_SIZE);
		 
		if(cwndTop - cwndBot >= ssThresh){
		    congAvoidanceFlag=1;
		    congAvoidValue = cwndTop+1;
		}
		continue;

	    }
	    // all packets ACKed
	    if(lastAckRecvd == incSeqNum(windowStartSeq, bodyLength)) {
		break;
	    }
	    // retransmit
	    if (packetsInWindow.count(lastAckRecvd) == 0) {
		cerr << "packet with sequence number " << lastAckRecvd
			     << " not in packetsInWindow" << '\n';
	    }
	    else {//PacketLoss
		cout << "Retransmitting: " << lastAckRecvd << endl;
		//packetsInWindow[lastAckRecvd].dump();
		//packetsInWindow[lastAckRecvd].sendto(sockfd, &other, other_length);
		ssThresh = (cwndTop-cwndBot+1)/2;
		cwndToSend = cwndBot;
		cwndTop = cwndBot+1;
		congAvoidanceFlag = 0;
		seqToSend = lastAckRecvd;
		wantedFile.seekg(cwndBot * DATA_SIZE, ios::beg);
	    }
	}
	

/*
	for (int totalFilePkts = 0; totalFilePkts < packsToSend; totalFilePkts += congWindowSize){ 

	    for (filepkts = 0; filepkts < congWindowSize; filepkts++, pktSent++) {

		memset(filebuf, 0, DATA_SIZE);

		//Read DATA_SIZE bytes normally, otherwise read the exact amount needed for the last packet
		int bytesToGet = ((filepkts == (packsToSend-1)) && (bodyLength % DATA_SIZE != 0))  ? (bodyLength % DATA_SIZE) : DATA_SIZE;
		wantedFile.read(filebuf, bytesToGet);
		toSend = TcpMessage(seqToSend, ackToSend, clientRecvWindow, "A");
		toSend.data = string(filebuf, bytesToGet);

		// store packet in case retransmission needed
		packetsInWindow[toSend.seqNum] = toSend;

		cerr << "sending packet " << filepkts << " of file: "<< filename << endl;
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
			cerr << "Retransmitting: " << lastAckRecvd << endl;
			//packetsInWindow[lastAckRecvd].dump();
			packetsInWindow[lastAckRecvd].sendto(sockfd, &other, other_length);
		}
	    }


	}


*/

	cout << "out of loop\n";

	/*
	for (int filepkts = 0; filepkts < packsToSend; filepkts++) {
		received.recvfrom(sockfd, &other, other_length);
		// receive ack for data
		cerr << "Received ack for data:\n";
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

	//hasReceivedFin = false;
	//while (!hasReceivedFin) {
		/* Should receive FIN from client */
	/*
		int r = received.recvfrom(sockfd, &other, other_length);
		if (r == RECV_TIMEOUT) {
			cerr << "Timeout while waiting for FIN\n";
			continue;
		}
		switch(received.flags) {
			case FIN_FLAG:
				// TODO: success
				cerr << "Received FIN\n";
				hasReceivedFin = true;
				break;
			default:
				cerr << "FIN wasn't received!\n";
				//exit(1);
		}
		received.dump();
	}
	*/

	cerr << "Shouldn't receive anything else from client now\n";
	//received.recvfrom(sockfd, &other, other_length);
	/* TODO: shouldn't receive anything from client so deal with cases where client sends stuff */
	
	close(sockfd);
	return 0;
}
