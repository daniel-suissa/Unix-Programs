
#include <signal.h>
#define MAX_MESSAGE_LENGTH 30 //this has to be >= 30 
#define MAX_NAME_LENGTH 5
#define MAX_TOTAL_LENGTH MAX_MESSAGE_LENGTH + MAX_NAME_LENGTH + 2 //includes a colon and a white space
#define DEFAULT_PORT 5197
#define END_OF_STATUS_TOKEN "ST_END"
#define DEBUGON 1
#define DEBUG if(DEBUGON)

void error_out(char *message);
void ignore_sigpipe();

void error_out(char *message){
	perror(message);
	exit(1);
}


void ignore_sigpipe(){
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) perror("signal failed, know that some funny stuff might happen");
}
