#include <signal.h>

#define BUFF_SIZE 10
#define TIMEOUT_SECS 600
#define EXIT_TOKEN ":exit"

void converse(int socket, char *myname); // given a socket, repeatedly prompt the user for input to send & read from socket data from other end
void errorOut(char *message); //print message and exit
void ignSigpipe(); // ignore SIGPIPE

void converse(int socket, char *myname){
	char stdinbuff[BUFF_SIZE];
	char socketbuff[BUFF_SIZE];
	char message[BUFF_SIZE];
	int maxfd = socket + 1;
	struct timespec timeout = {TIMEOUT_SECS, 0};
	int fd_num;
	int bytesRead;

	

	while(1){
		memset(&stdinbuff,0,BUFF_SIZE);
		memset(&socketbuff,0,BUFF_SIZE);
		memset(&message,0,BUFF_SIZE);

		fd_set input_set;
		FD_SET(STDIN_FILENO, &input_set);
		FD_SET(socket, &input_set);

		fd_num = pselect(maxfd, &input_set, NULL,NULL, &timeout, NULL);
		if (fd_num < 0) errorOut("pselect failed");
		if (fd_num == 0) errorOut("timeout");

		if(FD_ISSET(socket, &input_set)){
			
			if((bytesRead = read(socket, socketbuff, BUFF_SIZE)) == -1) errorOut("cannot read from socket");
			if(bytesRead == 0) {
				printf("peer hung up :(\n");
				fflush(stdout);
				break;
			}
			printf("%s", socketbuff);
			fflush(stdout);
		}
		if(FD_ISSET(STDIN_FILENO, &input_set)){
			if((bytesRead = read(STDIN_FILENO, stdinbuff, BUFF_SIZE)) == -1) errorOut("cannot read from stdin");
			if(bytesRead + strlen(myname) >= BUFF_SIZE){
				fprintf(stderr, "Message is too long, please break it into multiple messages");
				continue;
			}
			if(strncmp(stdinbuff, EXIT_TOKEN, strlen(EXIT_TOKEN)) == 0){
				close(socket);
				break;
			}
			sprintf(message, "%s: %s", myname, stdinbuff);
			if((write(socket, message, strlen(message))) == -1) errorOut("could not write to socket");
		}
	}
	
}

void errorOut(char *message){
	perror(message);
	exit(1);
}


void ignSigpipe(){
	struct sigaction sigpipeblock;
	sigpipeblock.sa_mask = 0;
	sigemptyset (&sigpipeblock.sa_mask);
	sigpipeblock.sa_handler = SIG_IGN;

	if (sigaction(SIGPIPE, &sigpipeblock, NULL) == -1) perror("sigaction failed, know that some funny stuff might happen");
}
