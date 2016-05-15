/*
    Simple udp client
    Silver Moon (m00n.silv3r@gmail.com)
*/
#include<stdio.h> //printf
#include<string.h> //memset
#include<stdlib.h> //exit(0);
#include<arpa/inet.h>
#include<sys/socket.h>
#include <unistd.h>
 
#define SERVER "10.0.0.1"
#define BUFLEN 512  //Max length of buffer
#define PORT 4000   //The port on which to send data
 
int main(void)
{
    struct sockaddr_in si_other;
    int s;
    socklen_t slen=sizeof(si_other);
    char message[BUFLEN];
 
    if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
	    printf("inet_aton");
	    exit(1);
    }
 
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(PORT);
     
    if (inet_aton(SERVER , &si_other.sin_addr) == 0) 
    {
	    printf("inet_aton");
	    exit(1);
    }
 
    strcpy(message, "asdf");

        //send the message
        if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1)
        {
	    printf("inet_aton");
	    exit(1);
        }
         
 
    close(s);
    return 0;
}
