CC = gcc

CFLAGS = -g -pthread

make: 
	$(CC) chatClient.c -o client $(CFLAGS)
	$(CC) chatServer.c -o server $(CFLAGS)

clean:
	$ rm *.o
