# TCP

This project created a TCP server and client on two different Vagrant virtual machines. The server and client send packets over UDP sockets, and the TCP features include flow control (through the client's receiver window), congestion control (slow start, congestion avoidance, and Reno fast recovery), and packet buffering.

`vagrant up` initializes the two virtual machines, and the files are located in the `/vagrant` directory in the virtual machines. `make` compiles the client and server to `./client` and `./server` respectively. These are used as follows:

 `./client SERVER-HOST-OR-IP PORT-NUMBER`
 
`./server PORT-NUMBER FILE-NAME`

The server is set up to wait until the client sets up a connection to it (IP is 10.0.0.3), and sends a file specified in the command line argument over 1032 byte packets to the client. 
