#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "util.h"

int getActiveSocket(char *address, int port); //set up an active socket conneted to a server with the provided address and port

int main(int argc, char **argv){

	if(argc < 2) errorOut("User name must be provided");

	long port = 80;
	char *address = "127.0.0.1";
	char *endptr;
	
	if (argc > 2){
		address = argv[2];
		
		if(argc > 3) {
			port = strtol(argv[3], &endptr, 10);
			if (errno == EINVAL) errorOut("port invalid");
		}
	}

	ignSigpipe();

	int socketfd = getActiveSocket(address, port);
	printf("Connected to %s\n", address);
	converse(socketfd, argv[1]);
	return 0;
}

int getActiveSocket(char *address, int port){
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd == -1) errorOut("couldn't not establish a socket");
	struct sockaddr_in serverAddress;
	memset(&serverAddress,0, sizeof(struct sockaddr_in));

	if (inet_pton(AF_INET, address, &serverAddress.sin_addr) <= 0) errorOut("inet_pton failed");
	serverAddress.sin_family      = AF_INET;          
    serverAddress.sin_port        = htons(port);

    if (connect(socketfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) errorOut("connect error");
    return socketfd;
}