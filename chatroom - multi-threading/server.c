#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "util.h" //includes the functions error_out and ignore_sigpipe

//macros
#define LISTENQ 20
//type definitions
struct d_client {
	int socket;
	char name[MAX_NAME_LENGTH];
	struct d_client *next;
	struct d_client *prev;
};
struct d_client;
typedef struct d_client d_client; 

struct message{
	char *text;
	char *name;
	d_client *sender;
	struct message *next;
	struct message *prev;
};
struct message;
typedef struct message message; 

//function definitions
void *client_listener(void *uncasted_client_ptr); // accepts and dequeues clients messages. adds and removes the client from the client list
void *message_clearer(void *arg); //dequeues messages from the queue and broadcasts them to clients
message *enqueue_message(char *message, char *sender_name, d_client *sender); //add a message to the back of the queue. returns enqueued message struct
message *enqueue_message_safe(char *message, char *sender_name, d_client *sender); //locks before calling enqueue_message, then unlocks
message *dequeue_message(); //returns and removes a message from the front of the queue. returns NULL on failure
void broadcast(char *sender_name, char *text, d_client *sender); //sends a foramtted message to all clients except for the sender
void free_message(message* a_message); //free up message's allocated memory and the message itself
d_client* add_client(int socket); //adds a client to the client list. returns the allocated client struct or NULL if failed
d_client* add_client_safe(int socket); //locks before calling add_client, then unlocks
void remove_client(d_client* client); //removes a client from the list and frees it
void remove_client_safe(d_client* client); //locks before calling remove_client, then unlocks
int report_clients(int socket); //report existing connected clients and writes them to the socket. returns 0 on success and -1 on failure
int report_clients_safe(int socket); //locks before calling report_clients, then unlocks
int get_passive_socket(int port); //// set up a passive socket that listens to new connections on port

//globals

//clients: 	hd <--> cl1 <--> cl2 <--> tl
d_client client_head = {-1, "HEAD", NULL,NULL};
d_client client_tail = {-1, "TAIL", NULL, &client_head};

//messages: back (where things are pushed) <--> msg1 <--> msg2 <--> msg3 <--> front (where things are pooped)
message front_of_message_queue = {NULL, NULL, NULL, NULL, NULL};
message back_of_message_queue = {NULL, NULL, NULL, &front_of_message_queue, NULL};

//mutexes and condition variables
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t messages_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t message_cond = PTHREAD_COND_INITIALIZER;

//rock n roll
int main(int argc, char **argv){

	//fetch port if one was given, otherwise use default port
	char *endptr;
	long port = DEFAULT_PORT;
	if(argc > 1){
		port = strtol(argv[1], &endptr, 10);
		if (errno == EINVAL) error_out("port invalid");
	}

	ignore_sigpipe(); //ignore sigpipe so that we're only informed that the server closed the socket from `read`

	//setup shared resources
	client_head.next = &client_tail;
	front_of_message_queue.prev = &back_of_message_queue;

	//set up passive socket
	int passive_socket = get_passive_socket(port);

	//start clearing thread (removes elements from the queue )
	pthread_t clearer_tid;
	if(pthread_create(&clearer_tid, NULL, message_clearer, NULL) != 0) error_out("could not start message cleaner thread");
	
	//start listening to new connections
	int active_socket;
	pthread_t new_client_tid;
	d_client *new_client_ptr;
	while ( (active_socket = accept(passive_socket, NULL, NULL)) >= 0){
		new_client_ptr = add_client_safe(active_socket);
		if (new_client_ptr == NULL) error_out("error adding a client");
		if (pthread_create(&new_client_tid, NULL, client_listener, (void*)new_client_ptr) != 0) error_out("could not add a client");
	}

	return 0;
}

void *client_listener(void *uncasted_client_ptr){
	//ignore all signale
	sigset_t mask;
	sigfillset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	d_client *client = (d_client*)(uncasted_client_ptr);
	DEBUG fprintf(stderr, "DEBUG\tclient connected to socket %d\n", client->socket);

	int bytes_read;
	char message_buffer[MAX_MESSAGE_LENGTH];

	printf("%s entered the chat room \n", client->name);

	//send status
	if (report_clients_safe(client->socket) == -1) error_out("could not report status to client");

	message *next_message;
	//first message enqueued is the one notifying that the client entered
	sprintf(message_buffer, "%s entered the chat room \n", client->name);
	next_message = enqueue_message_safe(message_buffer, "server", client);
	if (next_message == NULL) error_out("error enqueuing a message");

	//read client messages
	DEBUG fprintf(stderr, "DEBUG\tstarted listening on messages for %s \n", client->name);
	while(1){
		memset(&message_buffer, 0, MAX_MESSAGE_LENGTH);
		if((bytes_read = read(client->socket, message_buffer, MAX_MESSAGE_LENGTH)) == -1) error_out("error while reading from socket");
		if(bytes_read == 0){
			break;
		}
		//add message to the queue
		next_message = enqueue_message_safe(message_buffer, client->name, client);
		if (next_message == NULL) error_out("error enqueuing a message");

	}
	printf("%s left the chat room \n", client->name);

	//last message enqueued is the one notifying that the client left
	memset(&message_buffer, 0, MAX_MESSAGE_LENGTH);
	sprintf(message_buffer, "%s left the chat room \n", client->name);
	next_message = enqueue_message_safe(message_buffer, "server", client);

	//remove the client from the linked list & free the client ptr
	remove_client_safe(client);
	return NULL;
}

void *message_clearer(void *arg){
	//ignore all signals
	sigset_t mask;
	sigfillset(&mask);
	pthread_sigmask(SIG_SETMASK, &mask, NULL);

	DEBUG fprintf(stderr, "DEBUG\tmessage clearer started \n");
	while(1){
		pthread_mutex_lock(&messages_mutex);
		while(back_of_message_queue.next == &front_of_message_queue){
			pthread_cond_wait(&message_cond, &messages_mutex);
		}
		//get the last message available in the queue
		message *last_message = dequeue_message();
		pthread_mutex_unlock(&messages_mutex);

		if (last_message == NULL) error_out("error dequeuing a message"); //this shouldn't happen if things work correctly
		pthread_mutex_lock(&clients_mutex);
		broadcast(last_message->name, last_message->text, last_message->sender);
		free_message(last_message);
		pthread_mutex_unlock(&clients_mutex);
		DEBUG fprintf(stderr, "DEBUG\tmessage broadcasted \n");
	}
	return NULL;
} 

message *enqueue_message(char *message_text, char *sender_name, d_client *sender){
	DEBUG fprintf(stderr, "DEBUG\tenqueuing: %s \n", message_text);
	//create the message
	message *new_message = (message*)malloc(sizeof(message));
	if (new_message == NULL) return NULL;
	new_message->text = (char*)malloc(strlen(message_text) * sizeof(char*));
	if (new_message->text == NULL) return NULL;
	strcpy(new_message->text, message_text);
	new_message->name = (char*)malloc(strlen(sender_name) * sizeof(char*));
	if (new_message->name == NULL) return NULL;
	strcpy(new_message->name, sender_name);
	new_message->sender = sender;

	DEBUG fprintf(stderr, "DEBUG\tcreated message with text: %s and sender name: %s\n", message_text, sender_name);
	//enqueue
	new_message->next = back_of_message_queue.next;
	new_message->next->prev = new_message;
	back_of_message_queue.next = new_message;
	new_message->prev = &back_of_message_queue;

	return new_message;
}

message *enqueue_message_safe(char *message_text, char *sender_name, d_client *sender){
	pthread_mutex_lock(&messages_mutex);
	message* res = enqueue_message(message_text, sender_name, sender);
	pthread_cond_signal(&message_cond);
	pthread_mutex_unlock(&messages_mutex);
	return res;
}
message *dequeue_message(){
	DEBUG fprintf(stderr, "DEBUG\tdequeuing a message\n");
	message *last_message = front_of_message_queue.prev;
	if(last_message == &back_of_message_queue) return NULL;
	front_of_message_queue.prev = last_message->prev;
	last_message->prev->next = &front_of_message_queue;
	DEBUG fprintf(stderr, "DEBUG\tmessage dequeued\n");
	return last_message;
}

void broadcast(char *sender_name, char *text, d_client *sender){
	DEBUG fprintf(stderr, "DEBUG\tbroadcasting a message\n");
	//attach the sender's name to the message text
	char message_buffer[MAX_MESSAGE_LENGTH + MAX_NAME_LENGTH + 2];
	int bytes_written;
	if (sprintf(message_buffer, "%s: %s", sender_name,text) < 0) error_out("broadcast error");
	d_client *cursor = client_head.next;
	while(cursor != &client_tail){
		if(cursor != sender){ //do not broadcast to the sender of the message
			if ((bytes_written = write(cursor->socket, message_buffer, strlen(message_buffer))) == -1) error_out("broadcast error");
			if (bytes_written == 0){
				DEBUG fprintf(stderr, "client exitted\n");
			}
		}
		cursor = cursor->next;
	}
}

void free_message(message* a_message){
	free(a_message->text);
	free(a_message->name);
	free(a_message);
}
d_client* add_client(int socket){
	DEBUG fprintf(stderr, "DEBUG\tadd_client called\n"); 
	//create
	d_client *new_client = (d_client*)malloc(sizeof(d_client));
	if (new_client == NULL) return NULL; //note that the caller chooses what to do with the malloc error
	new_client->socket = socket;
	//new_client->name = "";
	memset(&(new_client->name), '\0', MAX_NAME_LENGTH);
	new_client->next = client_head.next;

	//attach
	new_client->next = client_head.next;
	new_client->next->prev = new_client;
	client_head.next = new_client;
	new_client->prev = &client_head;

	//get name
	int bytes_read = 0;
	//this read is guaranteed not to block
		//if the client is alive, the name will be sent first thing
		//if the client is dead, the read will return a 0
	if((bytes_read = read(socket, new_client->name, MAX_NAME_LENGTH)) < 1) error_out("error while reading client name");
	new_client->name[bytes_read] = '\0';
	DEBUG fprintf(stderr, "DEBUG\tgot a name: %s\n", new_client->name); 
	DEBUG fprintf(stderr, "DEBUG\tadd_client returning\n"); 
	return new_client;
}

d_client* add_client_safe(int socket){
	pthread_mutex_lock(&clients_mutex);
	d_client* new_client_ptr = add_client(socket);
	pthread_mutex_unlock(&clients_mutex);
	return new_client_ptr;
}

void remove_client(d_client* client){
	//remove from the list
	client->next->prev = client->prev;
	client->prev->next = client->next;
	if (close(client->socket)) error_out("error closing socket");
	free(client);
}

void remove_client_safe(d_client* client){
	pthread_mutex_lock(&clients_mutex);
	remove_client(client);
	pthread_mutex_unlock(&clients_mutex);
}
int report_clients(int socket){
	DEBUG fprintf(stderr, "DEBUG\treporting status to clients\n"); 
	d_client *curr_client = client_head.next;
	char status_buffer[MAX_TOTAL_LENGTH];
	memset(&status_buffer, '\0', MAX_TOTAL_LENGTH);
	sprintf(status_buffer, "server: Current participants: \n");
	if(write(socket, status_buffer, MAX_TOTAL_LENGTH) == -1) return -1;
	int counter = 1;
	memset(&status_buffer, '\0', MAX_TOTAL_LENGTH);
	while (curr_client != &client_tail){
		DEBUG fprintf(stderr, "DEBUG\tcurr client name: %s\n", curr_client->name); 
		sprintf(status_buffer, "%d.) %s\n", counter++, curr_client->name);
		if(write(socket, status_buffer, MAX_TOTAL_LENGTH) == -1) return -1;
		curr_client = curr_client->next;
	}
	return 0;
}
int report_clients_safe(int socket){
	pthread_mutex_lock(&messages_mutex);
	int res = report_clients(socket);
	pthread_mutex_unlock(&messages_mutex);
	return res;
}

int get_passive_socket(int port){

	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if(socketfd == -1) error_out("could not establish a socket");
	struct sockaddr_in server_address;
	memset(&server_address,0, sizeof(struct sockaddr_in));
	server_address.sin_family      = AF_INET;          
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_address.sin_port        = htons(port);

    if (bind(socketfd, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) error_out("could not bind socket to address");
    if (listen(socketfd, LISTENQ) == -1) error_out("cannot listen on socket");
    return socketfd;
}
