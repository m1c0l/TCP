#include <string>
#include <thread>
#include <iostream>
#include <sys/socket.h>

using namespace std;

int main(int argc, char **argv)
{
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

	return 0;
}
