#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#ifdef __linux__
	#define BLCKDIV 2
#else
	#define BLCKDIV 1
#endif

#define DEBUGON 0
#define DEBUG if(DEBUGON)

//binary tree structure
struct btree{
	struct btree* left;
	struct btree* right;
	int data;
};

struct btree;
typedef struct btree btree; 

btree* newbtree(int data); // creates a new btree node
int btree_search(btree* head, int data); // looks up data in head, returns 0 if nothing found
btree* btree_insert(btree* head, int data); //inserts a new node with data into a btree
void btree_free(btree* head); // frees all nodes from the btree

char** stackresize(char*** arr, int cap); // realloc'ing an array


int walkdir(DIR* dirp, btree* inodes_tree,  char* currdir); //recursively walking the directory tree
//most of the work happens in walkdir 


void errorout(char* message); //perrors the message and exits the process
void printpath(); //concantenates the elements of the global `pathstack` in a path format

char** pathstack; //stack used to keep directory names for printing
int stackdepth = 20;
int stacksize = 0;

int main(int argc, char** argv){
	char* dirname = ".\0";
	if (argc > 1){
		//change working directory if one was given as an argument
		if (chdir(argv[1]) == -1) errorout("Failed to open directory") ;
		dirname = argv[1];	
	}
	DIR* dirp;
	//open current working directory (which might be given by argument at this point)
	if((dirp = opendir(".")) == NULL) errorout("Failed to open directory");
	
	//setup inode numbers tree
	btree* tree = NULL;

	if((pathstack = (char**)malloc(stackdepth * sizeof(char*))) == NULL) errorout("Failed to malloc");
	
	walkdir(dirp, tree, dirname); // where the magic happens

	//free the inodes tree and path stack
	free(pathstack);
	btree_free(tree);
	return 0;
}

btree* newbtree(int data){
	btree* node;
	if ((node = (btree*)malloc(sizeof(btree))) == NULL) errorout("Failed to malloc");
 	node->data = data;
 	node->left = NULL;
 	node->right = NULL;
 	return node;
}

int btree_search(btree* head, int data){
	if (head == NULL){
		return 0;
	}
	if(data == head->data){
		return 1; //failure to insert
	} 
	if(data < (head)->data){
		return btree_search(head->left,data);
	}
	else{
		return btree_search(head->right,data);
	}
}

btree* btree_insert(btree* head, int data){
	if (head == NULL){
		return newbtree(data);
	}
	if(data == head->data){
		return NULL; //failure to insert
	} 
	if(data < (head)->data){
		head->left = btree_insert(head->left,data);
	}
	else{
		head->right = btree_insert(head->right,data);
	}
	return head;
}


void btree_free(btree* head){
	if(head != NULL){
		btree_free(head->left);
		btree_free(head->right);
		free(head);
	}
}

char** stackresize(char*** arr, int cap){
	if((*arr = (char**)realloc(*arr, cap * sizeof(char*))) == NULL) errorout("no memory to allocate");
	return *arr;
}

int walkdir(DIR* dirp, btree* inodes_tree, char* currdir){
	int diskusage = 0;
	struct dirent* next_entry;
	DIR* nextdirp;
	struct stat fileinfo; //stat buffer

	if(stacksize == stackdepth) pathstack = stackresize(&pathstack, stackdepth *= 2);
	if((pathstack[stacksize] = (char*)malloc(strlen(currdir)+1)) == NULL) errorout("Failed to malloc");
	strcpy(pathstack[stacksize++], currdir);

	
	while ((next_entry = readdir(dirp)) != NULL){
		if ( (lstat(next_entry->d_name, &fileinfo) == -1 ) ) errorout("File failure");

		//skip `..` entries
		if(strcmp(next_entry->d_name, "..") == 0) continue;

		//entry is a directory
		if(S_ISDIR(fileinfo.st_mode) && strcmp(next_entry->d_name, ".") != 0 ){
			if((nextdirp = opendir(next_entry->d_name)) == NULL) errorout("Failed to open directory");
			if (chdir(next_entry->d_name)== -1) errorout("Failed to walk directory tree");
			diskusage += walkdir(nextdirp, inodes_tree, next_entry->d_name);
			
			//change working directory back to where we came from
			//(from which you came, you shall remain)
			if (chdir("..") == -1) errorout("Failed to walk directory tree");

			closedir(nextdirp);

		} else { // entry is a regular file
			if (btree_search(inodes_tree, fileinfo.st_ino) == 0){
				inodes_tree = btree_insert(inodes_tree, fileinfo.st_ino);
				diskusage += fileinfo.st_blocks / BLCKDIV;
			}
		}
	}

	//print the path to this directory using the pathstack
	printf("%d\t", diskusage);
	printpath();
	free(pathstack[--stacksize]);
	return diskusage;
}


void errorout(char* message){
	perror(message);
	exit(1);
}

void printpath(){
	for (int i = 0; i < stacksize; i++){
		printf("%s", pathstack[i]);
		if(i < stacksize - 1) printf("/");
	}
	printf("\n");
	
}