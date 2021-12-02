#include <sys/socket.h>
#include <sys/types.h>
#include <resolv.h>
#include <pthread.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>

void panic(char *msg);
#define panic(m)	{perror(m); abort();}

typedef enum {
	ONLINE,
	WAS_ONLINE,
	AFK,
	OFFLINE
} clientStatus;

typedef struct _chat_socket_t
{
	int sd;
	char ID[17];
	pthread_t thread;
	struct _chat_socket_t* next;
	volatile clientStatus status;
}chat_socket_t;
chat_socket_t* socketHead = NULL;

void deleteLinkedSockets(){
	if(socketHead == NULL)
		return;

	chat_socket_t* next = socketHead->next;

	while (socketHead->next != NULL)
	{
		free(socketHead);
		socketHead = next;
	}
	
	free(socketHead);
}

void sendToAll(char* str, size_t len){
	chat_socket_t** socketHeadp = &socketHead; 
	while(*socketHeadp != NULL){
		if((*socketHeadp)->status != OFFLINE){
			send((*socketHeadp)->sd, str, len, 0);
			printf("\t-%d-> %s\n", (*socketHeadp)->sd, str);
		}
			
		socketHeadp = &((*socketHeadp)->next);
	}
}

void checkUpAll(){
	chat_socket_t** socketHeadp = &socketHead; 
	while(*socketHeadp != NULL){
		if((*socketHeadp)->status == ONLINE){
			send((*socketHeadp)->sd, "/:s", sizeof("/:s"), 0);
			printf("-%d-> %s\n", (*socketHeadp)->sd, "/:s");
			(*socketHeadp)->status = WAS_ONLINE;
			printf("Checkup for %d\n", (*socketHeadp)->sd);
		}

		socketHeadp = &((*socketHeadp)->next);
	}
}

void updateStatus(){
	chat_socket_t** socketHeadp = &socketHead; 
	while(*socketHeadp != NULL){
		switch ((*socketHeadp)->status)
		{
			//if thread received a reply, it updates its own status
			//if it wasn't updated, then the client is AFK
		case WAS_ONLINE: 
			(*socketHeadp)->status = AFK;
			printf("%s set to AFK\n", (*socketHeadp)->ID);
			char afkmsg[64];
			sprintf(afkmsg, "%s is now AFK.\n", (*socketHeadp)->ID);
			sendToAll(afkmsg, sizeof(afkmsg));
			break;
		default: //if it was ONLINE, the client just joined
			break;
		}

		socketHeadp = &((*socketHeadp)->next);
	}
}

typedef enum {
	SEND_CHECKUP,
	CHECK_REPLY
} checkupType;
checkupType _checkupType = SEND_CHECKUP; 

void alarm_handler(int signum){
	switch (_checkupType)
	{
	case SEND_CHECKUP:
		checkUpAll();

		alarm(4);
		_checkupType = CHECK_REPLY;	
		break;
	case CHECK_REPLY:
		updateStatus();

		alarm(1);
		_checkupType = SEND_CHECKUP;
		break;
	default:
		break;
	}
}

void term_handler(int signum){
	deleteLinkedSockets();
}

void *threadfunction(void *arg)                    
{	
	chat_socket_t *cst = (chat_socket_t*)arg;            /* get & convert the socket */
	printf("New thread with sd: %d\n", cst->sd);
	int sd = cst->sd;
	char buffer[256];
	
	strcpy(cst->ID, "UNDEFINED");

	recv(sd, cst->ID, sizeof(cst->ID), 0);
	printf("ID defined as: %s\n", cst->ID);

	while (1)
	{
		if(!recv(sd, buffer, sizeof(buffer), 0)) continue;

		printf("<-%d- %s\n", sd, buffer);
		char* message = buffer;
		
		//status message
		char* fence = strstr(buffer, "/:");
		if(fence != NULL) {
			message = fence + 2;
			if(strcmp(message, "on") == 0){
				cst->status = ONLINE;
			}
			else if(strcmp(message, "afk") == 0){
				cst->status = AFK;
			}
			continue;
		}
		//other messages
		fence = strstr(buffer, ":");
		if(fence != NULL){
			message = fence +1;
			if(strcmp(message, ":q") == 0) //quit message
				break;

			cst->status = ONLINE;
			sendToAll(buffer, sizeof(buffer)); //any other message
		}
	}

	shutdown(sd,SHUT_RD);
	shutdown(sd,SHUT_WR);
	shutdown(sd,SHUT_RDWR);
			                  /* close the client's channel */
	return 0;                           /* terminate the thread */
}

int main(int count, char *args[])
{	
	signal(SIGALRM, alarm_handler);
	signal(SIGTERM, term_handler);

	struct sockaddr_in addr;
	int listen_sd, port;

	if ( count != 2 )
	{
		printf("usage: %s <protocol or portnum>\n", args[0]);
		exit(0);
	}

	/*---Get server's IP and standard service connection--*/
	if ( !isdigit(args[1][0]) )
	{
		struct servent *srv = getservbyname(args[1], "tcp");
		if ( srv == NULL )
			panic(args[1]);
		printf("%s: port=%d\n", srv->s_name, ntohs(srv->s_port));
		port = srv->s_port;
	}
	else
		port = htons(atoi(args[1]));

	/*--- create socket ---*/
	listen_sd = socket(PF_INET, SOCK_STREAM, 0);
	if ( listen_sd < 0 )
		panic("socket");

	/*--- bind port/address to socket ---*/
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = port;
	addr.sin_addr.s_addr = INADDR_ANY;                   /* any interface */
	if ( bind(listen_sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
		panic("bind");

	/*--- make into listener with 10 slots ---*/
	if ( listen(listen_sd, 10) != 0 )
		panic("listen")

	/*--- begin waiting for connections ---*/
	else
	{	
		alarm(5); //start checkup alarm
		while (1)                         /* process all incoming clients */
		{
			int n = sizeof(addr);
			int sd = accept(listen_sd, (struct sockaddr*)&addr, &n);     /* accept connection */
			if(sd!=-1)
			{
				pthread_t child;

				chat_socket_t** socketHeadp = &socketHead; 
				while(*socketHeadp != NULL){
					socketHeadp = &((*socketHeadp)->next);
				}

				*socketHeadp = malloc(sizeof(chat_socket_t));
				(*socketHeadp)->sd = sd;
				(*socketHeadp)->thread = child;
				(*socketHeadp)->status = ONLINE;

				printf("New connection: sd = %d\n", sd);
				pthread_create(&child, 0, threadfunction, *socketHeadp);
				pthread_detach(child);
			}
		}

		deleteLinkedSockets();
	}
}
