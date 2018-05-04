#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "util.h"

#define	LISTENQ	0

int getPassiveSocket(int port); // set up a passive socket that listens to clients on port

int main(int argc, char **argv){

	if(argc < 2) errorOut("User name must be provided");

	long port = 1337;
	char *endptr;
	if(argc > 2) {
		port = strtol(argv[2], &endptr, 10);
		if (errno == EINVAL) errorOut("port invalid");
	}
	int passiveSocket = getPassiveSocket(port);
	int activeSocket;
	
	ignSigpipe();

	printf("waiting for a client..\n");;
	while ( (activeSocket = accept(passiveSocket, NULL, NULL)) >= 0){
		fprintf(stderr, "Got a client!\n");
		converse(activeSocket, argv[1]);
		printf("waiting for another client..\n");;
	}
	return 0;
}

int getPassiveSocket(int port){

	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd == -1) errorOut("couldn't not establish a socket");
	struct sockaddr_in serverAddress;
	memset(&serverAddress,0, sizeof(struct sockaddr_in));
	serverAddress.sin_family      = AF_INET;          
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); 
    serverAddress.sin_port        = htons(port);

    if (bind(socketfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) errorOut("could not bind socket to address");
    if (listen(socketfd, LISTENQ) == -1) errorOut("cannot listen on socket");
    return socketfd;
}

