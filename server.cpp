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
#include "TcpMessage.h"

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

	int data_inc = 1; //How much to increase the seq number by
	socklen_t other_length = sizeof(other);
	TcpMessage received;
	TcpMessage toSend;
	bool hasReceivedSyn = false;
	bool sendFile = false;
	uint16_t seqToSend = rand() % 0xffff; //The first sync
	uint16_t ackToSend;
	ifstream wantedFile;
	int packsToSend;
	int pktSent = 0;
	string flagsToSend = "";
	for (int packetsSent = 0; true; packetsSent++) {
	    cout << "Waiting for something" << endl;
		received.recvfrom(sockfd, &other, other_length);

		cout << "Packet arrived from" << inet_ntoa(addr.sin_addr)<< ": " << ntohs(other.sin_port) << endl;
		cout << "Received:" << endl;
		received.dump();


		//Send SYN-ACK if client is trying to set up connection
	      
		switch(received.flags){
		case SYN_FLAG:
		    if (hasReceivedSyn)
			{cerr <<"Multiple SYNs"; break;}
		    hasReceivedSyn = true;
		    flagsToSend ="SA";
		   
		    break;
		
		case SYN_FLAG | ACK_FLAG :
		    cerr << "Both SYN and ACK were set by client";
		    //exit(1);
		    break;

		case ACK_FLAG:
		    if (!hasReceivedSyn)
			{cerr<< "ACK before handshake"; break;}
		    flagsToSend = "A";
		    //Time to start sending the file back
		    sendFile = true;
		    
		    break;
		    
		default:
		    cerr << "Incorrect flags set";
		    break;
		}

		// if we're done handshake and are ready to send the file now
		if (sendFile) {
			break;
		}	      

		ackToSend = received.seqNum + 1;
		toSend = TcpMessage(seqToSend, ackToSend, BUFFER_SIZE, flagsToSend);
		toSend.sendto(sockfd, &other, other_length);
		cout << "Handshake: sending packet\n";
		toSend.dump();
	}
	
	char filebuf[DATA_SIZE]; //OHGODMAGICNUMBAAAHHHHHH
	wantedFile.open(filename);
	if (!wantedFile){
		perror("fstream open");
	}

	off_t bodyLength = 0;
	struct stat st; // of course C names a class and function the same thing...
	if(stat(filename.c_str(), &st) == -1) {
		perror("stat");
	} 
	bodyLength = st.st_size;
	packsToSend = ( 1+ (( bodyLength -1)/DATA_SIZE));

	// if the client sent data, increment ack by that number
	size_t recvLength = received.data.length();
	if (recvLength) {
		data_inc = recvLength;
	}	
	ackToSend = received.seqNum + data_inc;
	// increment our sequence number by 1
	seqToSend++; 

	for (int filepkts = 0; filepkts < packsToSend; filepkts++, pktSent++){

		memset(filebuf, 0, DATA_SIZE);

		//Read DATA_SIZE bytes normally, otherwise read the exact amount needed for the last packet
		int bytesToGet = ((filepkts == (packsToSend-1)) && (bodyLength % DATA_SIZE != 0))  ? (bodyLength % DATA_SIZE) : DATA_SIZE;
		wantedFile.read(filebuf, bytesToGet);
		string temp(filebuf, bytesToGet);
		toSend.data = temp;
		toSend = TcpMessage(seqToSend, ackToSend, 1034, "A");

		cout << "sending packet " << filepkts << " of file: "<< filename << endl;
		toSend.dump();
		toSend.sendto(sockfd, &other, other_length);

		// Increase sequence number to send next time by number of bytes sent
		seqToSend += bytesToGet;
	}

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

	/* send FIN */

	flagsToSend = "F";
	// Sequence number shouldn't change since we already increased by total data size
	//seqToSend = received.ackNum;
	//ackToSend = received.seqNum;
	ackToSend = 0; // ACK is invalid this packet
	toSend = TcpMessage(seqToSend, ackToSend, 1034, flagsToSend);
	toSend.sendto(sockfd, &other, other_length);
	cout << "Sending FIN\n";
	toSend.dump();

	/* Should receive FIN-ACK from client */
	received.recvfrom(sockfd, &other, other_length);
	switch(received.flags) {
		case FIN_FLAG | ACK_FLAG:
			// TODO: success
			cout << "Received FIN-ACK\n";
			break;
		default:
			cerr << "FIN-ACK wasn't received!\n";
			//exit(1);
	}
	received.dump();

	/* Should receive FIN from client */
	received.recvfrom(sockfd, &other, other_length);
	switch(received.flags) {
		case FIN_FLAG:
			// TODO: success
			cout << "Received FIN\n";
			break;
		default:
			cerr << "FIN wasn't received!\n";
			//exit(1);
	}
	received.dump();

	flagsToSend = "FA";
	seqToSend++;// increase sequence number by 1
	ackToSend = received.seqNum;
	toSend = TcpMessage(seqToSend, ackToSend, 1034, flagsToSend);
	toSend.sendto(sockfd, &other, other_length);
	cout << "Sending FIN-ACK\n";
	toSend.dump();

	cout << "Shouldn't receive anything else from client now\n";
	//received.recvfrom(sockfd, &other, other_length);
	/* TODO: shouldn't receive anything from client so deal with cases where client sends stuff */
	
	close(sockfd);
	return 0;
}
