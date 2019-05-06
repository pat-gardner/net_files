CFLAGS=-Wall -g 

all: watcher server

watcher: watcher.c net_files.h
	gcc $(CFLAGS) -o watcher.exe watcher.c

server: server.c net_files.h
	gcc $(CFLAGS) -o server.exe server.c

64: watcher64 server64

watcher64: watcher.c net_files.h
	gcc $(CFLAGS) -m32 -o watcher.exe watcher.c

server64: server.c net_files.h
	gcc $(CFLAGS) -m32 -o server.exe server.c

clean:
	rm *.exe
