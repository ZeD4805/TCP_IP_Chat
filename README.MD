# Chat app
Author: José Mendes

Student Number: b12744

### Requirements

- gcc

### Compilation
	
	make
### Usage

	./client <servername> <protocol or portnum> message1 message2 ...
	./server <protocol or portnum>

### Client
Gets asked for a nickname, and can start sending messages.
Will receive any new messages.

Responds to status update requests (checks if it is online)

### Server
Waits for any communication requests and services the connections by dispatching them to threads.

Every 5 seconds sends client update requests and waits up to 4 seconds to receive them.



