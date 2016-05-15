#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <thread>
#include <iostream>
#include <sys/socket.h>

const int BUFFER_SIZE = 1024;

using namespace std;

int main(int argc, char **argv) {
	if (argc != 3) {
		cerr << "usage: [PORT-NUMBER] [FILENAME]";
		return 1;
	}

	string port, filename;

	//We probably need default values, not sure what they are yet.
	port = argv[1];
	filename = argv[2]; 

	//Create UDP socket
	int sockfd = socket(AF_INET, SOCK_DGRAM,0);
	
	//Timeout flags and stuff could be set here
	int yes = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))== 1) {
	    perror("setsockopt");
	    return 1;
	}
	
	struct sockaddr_in addr, other;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(stoi(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);//inet_addr("127.0.0.1");
	
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));
       
	if(bind(sockfd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
	  perror("bind");
	  return 2;
	}
	int recv_length;
	unsigned char buffer[BUFFER_SIZE];
	socklen_t other_length = sizeof(other);
	while (true) {
	  cout << "Waiting for something" << endl;
	  recv_length = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *) &other, &other_length);
	  cout << "Recieved packet from" << inet_ntoa(addr.sin_addr)<< ":" << ntohs(other.sin_port) << endl;
	  cout << "Recieved:" <<buffer<<endl;
	  
	  //buffer = "ACK";
	  sendto(sockfd, buffer, recv_length, 0, (struct sockaddr*) &other, other_length);

	}

	return 0;
}
