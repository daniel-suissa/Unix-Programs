#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include "util.h"

//macros
#define DEFUALT_ADDRESS "127.0.0.1"
#define TIMEOUT_SECS 600
#define EXIT_TOKEN "quit"

int get_active_socket(char *address, int port); //set up an active socket conneted to a server with the provided address and port
void print_status_report(int socketfd); //get clients-status report from the server and print it to stdin
void converse(int socket);// given a socket, repeatedly prompt the user for input to send & read from socket data from other end
void close_socket_and_exit(int sig);

int socketfd = -1;

int main(int argc, char **argv){
	
	//validate arguments

	if(argc < 2) error_out("User name must be provided");
	if(strlen(argv[1]) + 1 > MAX_NAME_LENGTH) error_out("name provided is too long");
	long port = DEFAULT_PORT;
	char *address = DEFUALT_ADDRESS;
	char *endptr;
	if (argc > 2){
		address = argv[2];
		
		if(argc > 3) {
			port = strtol(argv[3], &endptr, 10);
			if (errno == EINVAL) error_out("port invalid");
		}
	}

	ignore_sigpipe(); //ignore sigpipe so that we're only informed that the server closed the socket from `read`

	socketfd = get_active_socket(address, port); //get a socket connected to the address at the port
	printf("Connected to %s on port %ld\n", address, port);
	//report client's name name
	DEBUG fprintf(stderr, "DEBUG\treporting my name \n");
	if(write(socketfd, argv[1], strlen(argv[1]) + 1) == -1) error_out("could not write name to socket");
	
	//get status and print 
	//print_status_report(socketfd);
	
	converse(socketfd);
	return 0;
}

int get_active_socket(char *address, int port){
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd == -1) error_out("couldn't not establish a socket");
	struct sockaddr_in serverAddress;
	memset(&serverAddress,0, sizeof(struct sockaddr_in));

	if (inet_pton(AF_INET, address, &serverAddress.sin_addr) <= 0) error_out("inet_pton failed");
	serverAddress.sin_family      = AF_INET;          
    serverAddress.sin_port        = htons(port);

    if (connect(socketfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) error_out("connect error");
    return socketfd;
}

/*
void print_status_report(int socketfd){
	char participants[MAX_NAME_LENGTH + MAX_MESSAGE_LENGTH + 2];
	int bytes_read = 0;
	int len_of_status_token = strlen(END_OF_STATUS_TOKEN);
	
	printf("Current participants in the conversation:\n");
	while(1){
		if ((bytes_read = read(socketfd, participants, MAX_NAME_LENGTH + MAX_MESSAGE_LENGTH + 2)) == -1) error_out("could not get participants");
		if(strcmp(participants + bytes_read - len_of_status_token, END_OF_STATUS_TOKEN) == 0 ){
			participants[bytes_read - len_of_status_token] = '\0';
			printf("%s", participants);
			fflush(stdout);
			break;
		}
		printf("%s", participants);
		fflush(stdout);

		memset(&participants, '\0', MAX_NAME_LENGTH + MAX_MESSAGE_LENGTH + 2);	
	}
	DEBUG fprintf(stderr, "DEBUG\tstatus reported \n");
}*/
void converse(int socket){
	char stdinbuff[MAX_MESSAGE_LENGTH];
	char socketbuff[MAX_MESSAGE_LENGTH + MAX_NAME_LENGTH + 2];
	struct timespec timeout = {TIMEOUT_SECS, 0};
	int maxfd = socket + 1;
	int fd_num;
	int bytesRead;

	while(1){
		memset(&stdinbuff,0,MAX_MESSAGE_LENGTH);
		memset(&socketbuff,0,MAX_TOTAL_LENGTH);

		fd_set input_set;
		FD_SET(STDIN_FILENO, &input_set);
		FD_SET(socket, &input_set);

		fd_num = pselect(maxfd, &input_set, NULL,NULL, &timeout, NULL);
		if (fd_num < 0) error_out("pselect failed");
		if (fd_num == 0) error_out("timeout");

		if(FD_ISSET(socket, &input_set)){
			
			if((bytesRead = read(socket, socketbuff, MAX_TOTAL_LENGTH)) == -1) error_out("cannot read from socket");
			if(bytesRead == 0) {
				printf("server hung up :(\n");
				fflush(stdout);
				break;
			}
			printf("%s", socketbuff);
			fflush(stdout);
		}
		if(FD_ISSET(STDIN_FILENO, &input_set)){
			if((bytesRead = read(STDIN_FILENO, stdinbuff, MAX_MESSAGE_LENGTH)) == -1) error_out("cannot read from stdin");
			if(bytesRead + 1 > MAX_MESSAGE_LENGTH) {
				printf("message is too long, please break and resend messages of max %d characters\n", MAX_MESSAGE_LENGTH - 1);
				fflush(stdout);
				while ((getchar()) != '\n');
				continue;
			}
			if(strncmp(stdinbuff, EXIT_TOKEN, strlen(EXIT_TOKEN)) == 0){
				close(socket);
				break;
			}
			if((write(socket, stdinbuff, strlen(stdinbuff))) == -1) error_out("could not write to socket");
		}
	}
	
}

void close_socket_and_exit(int sig){
	if(socketfd != -1){
		close(socketfd);
	}
	exit(1);
}
