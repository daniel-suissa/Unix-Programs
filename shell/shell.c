/*
Notes for Future Daniel:
	- support cd -
	- use current dir for prompt
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>


#define DEFAULT_PROMPT ">>> \0"
#define REDIRECT_OUPUT ">\0"
#define REDIRECT_ERR "2>\0"
#define REDIRECT_INPUT "<\0"
#define APPEND ">>\0"

#define COMMAND_NOT_FOUND_STATUS 127
#define CATCHALL_STATUS 1

//data structure to accumulate and modify the arguments array
typedef struct{
	char** arr;
	int cap; //capacity
	int n; // number of elements
} dynamicArgsArray;

void initArgsArray(dynamicArgsArray *args); //allocates the array and initialize cap and n
void resizeArgsArray(dynamicArgsArray *args); //allocates more space for the array and doubles the capacity 
void freeArgsArray(dynamicArgsArray *args); //frees the array
void argsAppend(dynamicArgsArray *args, char* val); // append val to the end of the array 

//represents a built-in operation and its associated function
typedef struct{
	char* command;
	void (*command_func)(int, char**);
} builtinOp;

void terminate(int argc, char **argv); //exit built in
void cd(int argc, char **argv); //change directory built in

void terminateChild(int sig); //signal handler. sends the signal to the child if it exists

void getArgs(dynamicArgsArray *args, char buffer[]); //builds args from buffer

void scanRedirections(dynamicArgsArray *args, int fds[], int fd_flags[]); //opens the files for redirections based on the redirection symbols
int findRedirectionSymbol(char* source); //returns a number between 1-3 that represents the redirection symbol in the string. 0 is no redirection 
void setupRedirections(int fds[], int default_fds[]); //closes the default files and dup2 if there are new files for redirection
void clearRedirections(int fds[], int default_fds[]);  //closes the redirection files (fds) if they are different from the default

void arrayCopy(int target[], int source[], int n); //copies n elements from source to target
void switchStatusVar(dynamicArgsArray args); //replaces the $? variable with the appropriate status code
void errorOut(char *message); //perrors the message and exits with status code 1
void printArr(char** arr); //mainly for debugging - prints arr

int builtins_size = 2;
builtinOp builtins[2] = { {"cd\0", &cd},{"exit\0", &terminate}};

int child_pid = 0;
int status = 0;

int main(int argc, char **argv){
	//set up SIGINT handeling
	struct sigaction redirect_to_child;
	sigemptyset (&redirect_to_child.sa_mask);
	redirect_to_child.sa_flags = SA_RESTART;
	redirect_to_child.sa_handler = &terminateChild;
	sigaction(SIGINT, &redirect_to_child, NULL);
	sigaction(SIGQUIT, &redirect_to_child, NULL);

	char* prompt; 
	if( ( prompt = getenv("PS1") ) == NULL){
		prompt = DEFAULT_PROMPT;
	} 

	dynamicArgsArray args = {NULL, 0, -1};
	initArgsArray(&args);
	char buffer[4096];
	int count_read;
	int fds[3];
	int fd_flags[3] = {O_RDONLY , O_WRONLY | O_CREAT, O_WRONLY | O_CREAT};
	int default_fds[3] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
	while(1){
		//clear and re-init the commands array
		freeArgsArray(&args);
		initArgsArray(&args);

		//print prompt
		printf("%s", prompt);
		fflush(stdout);

		//put the line in a buffer
		count_read = 0;
		if ((count_read = read(STDIN_FILENO, buffer, 4095)) > -1){
			buffer[count_read-1] = '\0'; // replace newline character with nullchar
		} 

		getArgs(&args, buffer);
		
		arrayCopy(fds, default_fds, 3);
		scanRedirections(&args, fds, fd_flags);
		if (fds[0] < 0 || args.n < 1) continue; //continue for next prompt if something went wrong with redirections

		//check for builtin commands and replace var $?
		int isbuiltin = 0;
		for(int i = 0; i < builtins_size; i++){
			if(strcmp(args.arr[0], builtins[i].command) == 0){
				(*(builtins[i].command_func))(args.n, args.arr);
				isbuiltin = 1;
			}
		}
		if(isbuiltin) continue;
		switchStatusVar(args);
		
		if(args.n > 0){
			child_pid = fork();
			if(child_pid == 0){
				//execute command, with redirections if there are any
				setupRedirections(fds,default_fds);
				execvp(args.arr[0],args.arr);
				status = COMMAND_NOT_FOUND_STATUS;
				errorOut("exeq failed");
			}
			else {

				//wait on the child and close any descriptors that stayed open
				wait(&status);
				if (WIFEXITED(status)){
					status = WEXITSTATUS(status);
				}
				else if (WIFSIGNALED(status)){
					status = 128 + WTERMSIG(status);
				}
				else{ // WIFSTOPPED(status) must be true
					status = WSTOPSIG(status);
				}

				fflush(stdout);
				clearRedirections(fds, default_fds);
			}
		}
	}
	return 0;
}

void terminate(int argc, char **argv){
	exit(1);
}
void cd(int argc, char **argv){

	// case 1: no argument is given or args is '~'  -> cd to home directory
	if(argc < 2 || strcmp(argv[1], "~\0") == 0){
		if(chdir(getenv("HOME")) == -1) {
			status = CATCHALL_STATUS;
			perror("Oops. Couldn't cd into home directory");
		}
	}
	//case 2: an directory is explicitly given, cd into that directory
	else if(argc > 1 && chdir(argv[1]) == -1) {
		status = CATCHALL_STATUS;
		perror("Oops. Couldn't cd");
	}
}

void terminateChild(int sig){
	if(child_pid != 0) kill(child_pid, sig);
	printf("\n");
	fflush(stdout);
}

void initArgsArray(dynamicArgsArray *args){
	args->cap = 10;
	if((args->arr = (char**)malloc(args->cap * sizeof(char*))) == NULL) errorOut("no memory to allocate");
	args->n = 0;
}
 
void resizeArgsArray(dynamicArgsArray *args){
	args->cap *= 2;
	if((args->arr = (char**)realloc(args->arr, args->cap * sizeof(char*))) == NULL) errorOut("no memory to allocate");
}
void freeArgsArray(dynamicArgsArray *args){
	free(args->arr);
}

void argsAppend(dynamicArgsArray *args, char* val){
	if (args->n - 1 == args->cap) resizeArgsArray(args);
	args->arr[args->n] = val;
	(args->n)++;
}

void getArgs(dynamicArgsArray *args, char buffer[]){

	char *nextCommand;
	nextCommand = strtok(buffer, " ");
	while (nextCommand != NULL){
		argsAppend(args, nextCommand);
		nextCommand = strtok(NULL, " ");
		
	}
	argsAppend(args, nextCommand); //include the NULL entry
	(args->n)--;
}

void scanRedirections(dynamicArgsArray *args, int fds[], int fd_flags[]){
	int error[3] = {-1, -1, -1};
	int default_fds[3] = {STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO};
	int mapped_fd;
	int maybeAppend = 0;
	for(int i = 0 ;i < args->n - 1; i++){
		if ((mapped_fd = findRedirectionSymbol(args->arr[i])) > -1 ) {
			if (mapped_fd == 3) {
				mapped_fd = 1;
				maybeAppend = O_APPEND;
			}
			if (fds[mapped_fd] != default_fds[mapped_fd]) close(fds[mapped_fd]);
			
			if(i == args->n - 1){
				fprintf(stderr, "-shell: output redirected to nowhere\n");
				arrayCopy(fds, error, 3);
				break;
			}

			if ( (fds[mapped_fd] = open(args->arr[i+1], fd_flags[mapped_fd] | maybeAppend, S_IRWXU)) == -1) {
				perror("could not open file");
				arrayCopy(fds, error, 3);
				break;
			}

			for(int j = i; j < args->n - 1; j++){
				args->arr[j] = args->arr[j + 2];
			}
			args->n -= 2;
		}
	}
}

int findRedirectionSymbol(char* source){
	if (strcmp(source, REDIRECT_INPUT) == 0){
		return 0;
	}
	if (strcmp(source, REDIRECT_OUPUT) == 0){
		return 1;
	}
	if (strcmp(source, REDIRECT_ERR) == 0){
		return 2;
	}
	if (strcmp(source, APPEND) == 0){
		return 3;
	}
	return -1;
}

void setupRedirections(int fds[], int default_fds[]){
	for(int i = 0; i < 3; i++){
		if (fds[i] != default_fds[i]){
			close(default_fds[i]);
		if(dup2(fds[i], default_fds[i]) == -1) errorOut("dup2 failed");
		}
	}
}

void clearRedirections(int fds[], int default_fds[]){
	for(int i = 0; i < 3; i++){
		if (fds[i] != default_fds[i]){
			close(fds[i]);
		}
	}
}

void arrayCopy(int target[], int source[], int n){
	for(int i = 0; i < n; i++){
		target[i] = source[i];
	}
}

void switchStatusVar(dynamicArgsArray args){
	char status_str[4];
	for(int i = 0; i < args.n; i++){
		if(strcmp(args.arr[i],"$?\0") == 0){
			sprintf(status_str, "%d", status);
			strcpy(args.arr[i], status_str);
		}
	}
}

void errorOut(char *message){
	perror(message);
	exit(1);
}

void printArr(char** arr){
	int i = 0;
	for(; arr[i+1] != NULL; i++){
		fprintf(stderr, "%s ", arr[i]);
	}
	fprintf(stderr, "%s\n", arr[i]);
}