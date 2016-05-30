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
#include "TcpMessage.h"


#include <thread>
#include <iostream>
#include <sys/socket.h>

const int BUFFER_SIZE = 1032;
const int DATA_SIZE = 1024;

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
	int send_length;
	int data_inc = 1; //How much to increase the seq number by
	char buf[BUFFER_SIZE];
	socklen_t other_length = sizeof(other);
	TcpMessage received;
	TcpMessage toSend;
	bool hasReceivedSyn = false;
	bool sendFile = false;
	uint16_t seqToSend = rand() % 65536; //The first sync
	uint16_t ackToSend;
	ifstream wantedFile;
	int packsToSend;
	int pktSent = 0;
	int msg_len; //Length of ultimate packet to send
//	for (int packetsSent = 0; true ; packetsSent++) {
	int packs_to_send;
	string flagsToSend = "";
	//	for (int packetsSent = 0; true ; packetsSent++) {
	while (true){
	    cout << "Waiting for something" << endl;
	    if ((recv_length = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (sockaddr *) &other, &other_length)) == -1) {
		perror("recvfrom");
	    }

		//Obtain header from receive
		received.bufferToMessage(buf, recv_length);

		cout << "Packet arrived from" << inet_ntoa(addr.sin_addr)<< ": " << ntohs(other.sin_port) << endl;
		cout << "Received:" << endl;
		received.dump();


		//Send SYN-ACK if client is trying to set up connection
	      
		
		//bool receivedSyn = received.getFlag('s');
		//bool receivedAck = received.getFlag('a');

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
	      
		//If not the first packet
		if (received.flags != SYN_FLAG) { 
			seqToSend = received.ackNum;
			data_inc = received.data.length() ? 1 : received.data.length();   
		}
		ackToSend = received.seqNum + data_inc;

		toSend = TcpMessage(seqToSend, ackToSend, BUFFER_SIZE, flagsToSend);

	
		if (sendFile){
		    char filebuf[DATA_SIZE]; //OHGODMAGICNUMBAAAHHHHHH
		    wantedFile.open(filename);
		    if (!wantedFile){
		 	perror("fstream open");
		    }
		    
		    off_t bodyLength = 0;
		    struct stat st;
		    if(stat(filename.c_str(), &st) == -1) {
			perror("stat");
		    } 
		    bodyLength = st.st_size;
		    packsToSend =( 1+ (( bodyLength -1)/DATA_SIZE));
		    
		   
		    
		    for (int  filepkts = 0; filepkts < packsToSend; filepkts++, pktSent++){
		    
		    // while(wantedFile){
			memset(filebuf, 0, DATA_SIZE);

			//Read 1024 bytes normally, otherwise read the exact amount needed for the last packet
			int bytesToGet = ((filepkts == (packsToSend-1)) && (bodyLength % DATA_SIZE != 0))  ? (bodyLength % DATA_SIZE) : 1024; 
			wantedFile.read(filebuf, bytesToGet);
			string temp(filebuf);
			toSend.data = temp;
			toSend.seqNum = ackToSend + filepkts*bytesToGet;
			
			msg_len = toSend.messageToBuffer(buf);

			cout << "sending packet " << filepkts << " of file: "<< filename << endl;
			if((send_length = sendto(sockfd, buf, msg_len, 0, (sockaddr*) &other, other_length)) == -1) {
			    perror("sendto");

			}
				
			toSend.dump();
			
		    }
		    continue;
		}

		

		//Send packets to client 
		if((send_length = sendto(sockfd, buf, msg_len, 0, (sockaddr*) &other, other_length)) == -1) {
			perror("sendto");
		}
		//	sendto(sockfd, buf, recv_length, 0, (sockaddr*) &other, other_length);

	}

	/* send FIN */

	flagsToSend = "F";
	seqToSend = received.ackNum;
	/* This isn't needed since it's not valid anyways, but it makes it easier for client to send the same number back.*/
	ackToSend = received.seqNum;
	toSend = TcpMessage(seqToSend, ackToSend, 1034, flagsToSend);
	int hdrLen = toSend.messageToBuffer(buf);
	if ((send_length = sendto(sockfd, buf, hdrLen, 0, (sockaddr*) &other, other_length)) == -1) {
		perror("sendto");
	}

	if ((recv_length = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (sockaddr *) &other, &other_length)) == -1) {
		perror("recvfrom");
	}
	/* Should receive FIN-ACK from client */
	switch(received.flags) {
		case FIN_FLAG | ACK_FLAG:
			// TODO: success
			break;
		default:
			cerr << "FIN-ACK wasn't received!";
			exit(1);
	}

	/* Should receive FIN from client */
	if ((recv_length = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (sockaddr *) &other, &other_length)) == -1) {
		perror("recvfrom");
	}
	/* Should receive FIN-ACK from client */
	switch(received.flags) {
		case FIN_FLAG:
			// TODO: success
			break;
		default:
			cerr << "FIN wasn't received!";
			exit(1);
	}
	flagsToSend = "FA";
	seqToSend = received.ackNum;
	ackToSend = received.seqNum;
	toSend = TcpMessage(seqToSend, ackToSend, 1034, flagsToSend);
	hdrLen = toSend.messageToBuffer(buf);
	if ((send_length = sendto(sockfd, buf, hdrLen, 0, (sockaddr*) &other, other_length)) == -1) {
		perror("sendto");
	}

	if ((recv_length = recvfrom(sockfd, buf, BUFFER_SIZE, 0, (sockaddr *) &other, &other_length)) == -1) {
		perror("recvfrom");
	}
	/* TODO: shouldn't receive anything from client so deal with cases where client sends stuff */
	
	close(sockfd);
	return 0;
}
