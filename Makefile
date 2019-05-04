CFLAGS=-Wall -g

all: watcher socket

watcher: watcher.c
	gcc $(CFLAGS) -o watcher.exe watcher.c

socket: my_socket.h client_inet.c server_inet.c
	gcc $(CFLAGS) -o client_inet.exe client_inet.c
	gcc $(CFLAGS) -o server_inet.exe server_inet.c

clean:
	rm watcher.exe client_inet.exe server_inet.exe
