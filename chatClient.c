
/*****************************************************************************/
/*** tcpclient.c                                                           ***/
/***                                                                       ***/
/*** Demonstrate an TCP client.                                            ***/
/*****************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <resolv.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>


void panic(char *msg);
#define panic(m)	{perror(m); abort();}
/****************************************************************************/
/*** This program opens a connection to a server using either a port or a ***/
/*** service.  Once open, it sends the message from the command line.     ***/
/*** some protocols (like HTTP) require a couple newlines at the end of   ***/
/*** the message.                                                         ***/
/*** Compile and try 'tcpclient lwn.net http "GET / HTTP/1.0" '.          ***/
/****************************************************************************/

typedef struct _receiverThreadArg
{
	int sd;
	char* ID;
}receiverThreadArg;


void* receiverThread(void* arg){
	receiverThreadArg rta = *(receiverThreadArg*)arg;
	int sd = rta.sd;
	char buffer[256] = {0};
	char statusReply[17 + 5];

	strcpy(statusReply, rta.ID);
	int len = strlen(statusReply);
	strcpy(statusReply + len, "/:on"); //online, "/:afk" for afk

	//	SENDER:MESSAGE
	do{
		recv(sd, buffer, sizeof(buffer), 0);
		if(strcmp(buffer, "/:s") == 0){
			strcpy(buffer, rta.ID);

			strcpy(statusReply + len, "/:on"); //online, "/:afk" for afk
			//TODO implement afk
			send(sd, statusReply, sizeof(statusReply), 0);
			continue;
		}

		if(strcmp(buffer, "/:q") == 0)
			break;

		char* message = buffer;

		
		char* fence = strstr(buffer, "/:"); //command
		if(fence != NULL){
			*fence = '\0';
			message = fence + 2;
		}
		else{
			fence = strstr(buffer, ":"); //regular message
			if(fence != NULL) {
				*fence = '\0';
				message = fence + 1;
			}
		}
		
		if(strcmp(buffer, rta.ID) != 0){ //not from sender
			printf("%s sent: %s\n", buffer, message);
		}

	}while(1);
}

int main(int count, char *args[])
{	struct hostent* host;
	struct sockaddr_in addr;
	int sd, port;

	if ( count < 3 )
	{
		printf("usage: %s <servername> <protocol or portnum>\n", args[0]);
		exit(0);
	}

	/*---Get server's IP and standard service connection--*/
	host = gethostbyname(args[1]);
	//printf("Server %s has IP address = %s\n", args[1],inet_ntoa(*(long*)host->h_addr_list[0]));
	if ( !isdigit(args[2][0]) )
	{
		struct servent *srv = getservbyname(args[2], "tcp");
		if ( srv == NULL )
			panic(args[2]);
		printf("%s: port=%d\n", srv->s_name, ntohs(srv->s_port));
		port = srv->s_port;
	}
	else
		port = htons(atoi(args[2]));

	/*---Create socket and connect to server---*/
	sd = socket(PF_INET, SOCK_STREAM, 0);        /* create socket */
	if ( sd < 0 )
		panic("socket");
	memset(&addr, 0, sizeof(addr));       /* create & zero struct */
	addr.sin_family = AF_INET;        /* select internet protocol */
	addr.sin_port = port;                       /* set the port # */
	addr.sin_addr.s_addr = *(long*)(host->h_addr_list[0]);  /* set the addr */

	/*---If connection successful, send the message and read results---*/
	if ( connect(sd, (struct sockaddr*)&addr, sizeof(addr)) == 0)
	{	
        char buffer[256]={0};
		char ID[17];
		
		do{
			printf("Nickname (\"/\" is forbidden): \n");
			scanf("%17s", ID);
			//printf("Len: %d, str: %s\n", strlen(ID), ID);
		}while(strlen(ID) == 0 || (strlen(ID) == 1 && ID[0] == '/'));


		send(sd, ID, sizeof(ID), 0);
		printf("\n");
		sprintf(buffer, "%s:", ID);
		int len = strlen(buffer);
		char* message = buffer + len;
		
		receiverThreadArg rt_arg;
		rt_arg.sd = sd;
		rt_arg.ID = ID;

		pthread_t receiver;
		pthread_create(&receiver, 0, receiverThread, &rt_arg);
		pthread_detach(receiver);

		for(int i = 3; i < count; i++){
			strcpy(message, args[i]);
			send(sd, buffer, sizeof(buffer), 0);
			printf("Sent: %s\n", buffer);
		}
		
		while(1){
			scanf("%239s", message); //256 - 17 (ID + ':')

			if(strcmp(message, ":q") == 0)
				break;

			send(sd, buffer, sizeof(buffer), 0);	/* send message */
		}
	}
	else
		panic("connect");
}
