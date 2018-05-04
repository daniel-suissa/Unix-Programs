#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ftw.h>

#define DEBUGON 1
#define DEBUG if(DEBUGON)

struct btree{
	struct btree* left;
	struct btree* right;
	int data;
};

struct btree;
typedef struct btree btree; //yep I know this looks weird, apparently it's the way to do recursive structs

void btree_init(btree** tree);
int btree_insert(btree* head, int data); //TODO: handle concurrency
void btree_free(btree* head);

int diskusage;
btree* inodes_tree;

int func(const char *fpath, const struct stat *sb,
            int typeflag, struct FTW *ftwbuf){
	struct stat fileinfo;
	if(typeflag == FTW_F || typeflag == FTW_SL){
		lstat(fpath, &fileinfo);
		if (btree_insert(inodes_tree, fileinfo.st_ino) == 1){
			DEBUG fprintf(stderr,"filename: %s ,inode no: %llu , isdir?: %d \n", fpath, fileinfo.st_ino, S_ISDIR(fileinfo.st_mode));
			diskusage += fileinfo.st_blocks;
		}
	}
	return 0;
}

int main(int argc, char** argv){
	diskusage = 0;
	int flags = 0;
	btree_init(&inodes_tree);

	if (nftw((argc < 2) ? "." : argv[1], &func, 20, flags) == -1)
    {
        perror("nftw");
        exit(EXIT_FAILURE);
    }
    btree_free(inodes_tree);
    printf("%d\n", diskusage);
	return 0;
}


void btree_init(btree** tree){
	if(  (*tree = (btree*)malloc(sizeof(btree))) == NULL ) {
		perror("Failed to malloc");
		exit(1);
	}
	btree leaf = (btree){NULL,NULL,-1};
	**tree = leaf;
}

int btree_insert(btree* head, int data){
	//DEBUG fprintf(stderr, "INSERT %d, head->data is %d,\n", data, head->data);
	btree* tmp;
	if (head->data == -1){
		//place data
		head->data = data;
		//DEBUG fprintf(stderr, "placed %d in tree\n", data);

		//create leaves
		if( (tmp = (btree*)malloc(sizeof(btree))) == NULL ) {
			perror("Failed to malloc");
			exit(1);
		}
		head->left = tmp;
		btree leaf = (btree){NULL,NULL,-1};
		*(head->left) = leaf;

		if( (tmp = (btree*)malloc(sizeof(btree))) == NULL ) {
			perror("Failed to malloc");
			exit(1);
		}
		head->right = tmp;
		*(head->right) = leaf;
		tmp = NULL;


		return 1; //insertion successful
	}
	if(data == head->data){
		return 0; //failure to insert
	} 
	if(data < head->data){
		return btree_insert(head->left, data);
	}
	else{
		return btree_insert(head->right, data);
	}
}

void btree_free(btree* head){
	if(head != NULL){
		btree_free(head->left);
		btree_free(head->right);
		free(head);
	}
}