#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include "TcpMessage.h"

using namespace std;
 
#define BUFFER_SIZE 512  //Max length of buffer
 
int main(int argc, char **argv)
{
    if (argc != 3) {
		cerr << "usage: " << argv[0] << " SERVER-HOST-OR-IP PORT-NUMBER" << '\n';
        exit(1);
    }

    string ip = argv[1];
    string port = argv[2];

	srand (time(NULL)); //Used to generate random ISN
 
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
		exit(1);
    }
 
    sockaddr_in si_other;
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(stoi(port));
     
    if (inet_aton(ip.c_str() , &si_other.sin_addr) == 0) {
        perror("inet_aton");
	    exit(1);
    }
 
    TcpMessage testSend(rand() % 65536, 0, 1034, "S");

    char test[BUFFER_SIZE];
    testSend.messageToBuffer(test);

    cout << "sending:" << endl;
    testSend.dump();
    
    //send the message
    //size = messagesize+1 for null byte
    if (sendto(sockfd, test, 8, 0,
                (sockaddr*)&si_other, sizeof(si_other)) == -1) {
        perror("sendto");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    socklen_t other_length = sizeof(si_other);

    int recv_length = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&si_other, &other_length);
    if (recv_length == -1) {
        perror("recvfrom");
        exit(1);
    }

    //cout.write(buffer, recv_length);
    //cout.flush();
    TcpMessage received;
    cout << "receiving:" << endl;
    received.bufferToMessage(buffer, recv_length);
    received.dump();
    
 
    close(sockfd);
    return 0;
}
