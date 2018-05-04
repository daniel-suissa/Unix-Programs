#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUGON 0
#define DEBUG if(DEBUGON)
extern char** environ;


typedef struct {
	int vars_start;          //index in argv where the key-value pairs start
	int vars_end;
	int ignore;              //ignore environment       
	char* command_argv;      // the argv of the command passed
} arguments;

int read_key_value_pairs(int argc, char** argv, int start); // reading name=value and returning the index where stopped
arguments get_args(int argc, char** argv); // converts argv into the argument struct
int get_var_key(char** env, char* var, int count); // finds the index of a variable var in env
int get_equal_index(char* str); //finds the equal sign in a string
int resize(char*** arr, int cap); // resizing an array
void fillenviron(char** environ, char** argv, int start, int count); //copies count pointers from argv into environ
int getsize(char** arr); //get size of a NULL terminated array
void printarr(char** arr); // prints the contents of a NULL terminated array
void put_environ_on_heap(char*** environ, int environcap); //copies environ to an array on the heap and moves environ to point there

int main(int argc, char** argv){
	
	arguments processed_args = get_args(argc, argv); //set up the struct containing info about the arguments

	int vars_size = processed_args.vars_end - processed_args.vars_start; //how many env variables were passed in
	DEBUG fprintf(stderr, "vars start: %d, vars end: %d, command: %s\n", processed_args.vars_start, processed_args.vars_end, processed_args.command_argv);
	DEBUG fprintf(stderr, "--------\n");
	int environmalloc = 0; // flag to tell whether environ needs to be free'd

	if(processed_args.ignore){
		//-i argument given. empty out environ, accumulate a count of the size
		DEBUG fprintf(stderr, "environ ignored\n");
		if ((environ = malloc(vars_size + 1)) == NULL){
			fprintf(stderr, "no memory to allocate\n");
			exit(1);
		}
		environmalloc = 1;
		fillenviron(environ, argv, processed_args.vars_start, vars_size);
	} else{
		// use current environment. Replace / Add any name=value pairs given

		DEBUG fprintf(stderr, "environ not ignored\n");
		//find out environ size and capacity, initially assume they are the same
		int environsize = getsize(environ);
		int environcap = environsize;

		int equalindex;
		int varindex;
		//loop the argv key-value vars and either modify environ or add elements to it
		for(int i = 0; i < vars_size; i++){
			equalindex = get_equal_index(argv[processed_args.vars_start + i]); // find the index of the '=' sign
			DEBUG printf("index of = for string %s is %d", argv[processed_args.vars_start + i], equalindex);

			//if key is in environ, replace it
			if ((varindex = get_var_key(environ, argv[processed_args.vars_start + i], equalindex + 1)) != -1){
				DEBUG fprintf(stderr, "variable %s found at index %d of environ\n", argv[processed_args.vars_start + i], varindex);
				environ[varindex] = argv[processed_args.vars_start + i];
			}
			//if key is missing, add the variable to the end of environ
			else{
				DEBUG fprintf(stderr, "variable %s not found\n", argv[processed_args.vars_start + i]);
				if(environcap <= environsize + 2){
					//if not enough room in environ - get new environ and new capacity
					environcap *= 2;
					if (environmalloc == 1){
						resize(&environ, environcap);
					} else{
						put_environ_on_heap(&environ, environcap);
						environmalloc = 1;
						
					}
					environ[environsize] = NULL;
				}
				environ[environsize++] = argv[processed_args.vars_start + i];
			}
		}
	}

	DEBUG fprintf(stderr, "---------now business----------\n");
	if (processed_args.command_argv != NULL){
		execvp(processed_args.command_argv, argv + processed_args.vars_end);
	} else{
		printarr(environ);
	}
	if(environmalloc == 1){
		free(environ);
	}
	return 0;
}

void put_environ_on_heap(char*** environ, int environcap){
	DEBUG fprintf(stderr, "putting environ on the heap\n" );
	char** tmp = *environ;
	if((*environ = (char**)malloc(environcap * sizeof(char*))) == NULL){
		fprintf(stderr, "no memory to allocate\n");
		exit(1);
	}
	for (int i = 0; tmp[i] != NULL; i++){
		(*environ)[i] = tmp[i];
	}
	tmp = NULL;
}
int read_key_value_pairs(int argc, char** argv, int start){
	int count = 0;
	while(start + count < argc){
		if (strchr(argv[start + count], '=') != NULL){
			count++;
		}
		else{
			break;
		}
	}
	return count;
}

arguments get_args(int argc, char** argv){
	//puts("getting args..");
	arguments args = {1,1,0,NULL};
	//puts("made it to initilize args");
	int i = 0;
	if (argc > 1) {
		i = 1;
		if(strncmp(argv[i],"-i",2) == 0) {
			args.ignore = 1;
			i++;
			args.vars_start++;
			args.vars_end++;
		}
		args.vars_end = args.vars_start + read_key_value_pairs(argc,argv,i);
	}
	args.command_argv = argv[args.vars_end]; // this would be either NULL or a command

	return args;
}

int get_var_key(char** env, char* var, int count){
	int i = 0;
	while (env[i] != NULL){
		if(strncmp(env[i], var, count) == 0){
			return i;
		}
		i++;
	}
	return -1;
}

int get_equal_index(char* str){
	int i = 0;
	while (str[i] != '\0'){
		if(str[i] == '='){
			return i;
		}
		i++;
	}
	return -1;
}

int resize(char*** arr, int cap){
	DEBUG fprintf(stderr, "resizing...\n");
	if((*arr = (char**)realloc(*arr, cap * sizeof(char*))) == NULL){
		fprintf(stderr, "no memory to allocate\n");
		exit(1);
	};
	return cap;
}

void fillenviron(char** environ, char** argv, int start, int count){
	for(int i = 0; i < count ; i++){
		environ[i] = argv[start + i];
	}
	environ[count] = NULL;
}

int getsize(char** arr){
	int size = 0;
	for (int i = 0; arr[i] != NULL; i++){
		size++;
	}
	return size;
}

void printarr(char ** arr){
	for(int i = 0; arr[i] != NULL; i++){
		printf("%s\n", arr[i]);
	}
}