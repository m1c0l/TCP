#include <string>
#include <thread>
#include <iostream>

using namespace std;

int main(int argc, char **argv)
{
	if (argc != 3) {
		cerr << "usage: ./client SERVER-HOST-OR-IP PORT-NUMBER";
		return 1;
	}
	string hostStr, port;
	hostStr = argv[1];
	port = argv[2];
}
