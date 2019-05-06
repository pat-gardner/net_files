#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <netinet/ip.h>
#include <arpa/inet.h>

#include "watcher.h"

#define BUF_LEN 4096
#define NUM_ARGS 2

int main(int argc, char** argv) {
	int sock_fd, data_fd, port, ret;
	struct sockaddr_in inet_addr, client_addr;
	socklen_t peer_addr_size;
	char read_buf[BUF_LEN];
	char* MY_IP;
    enum f_event event_type;

    if (argc != NUM_ARGS + 1) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    MY_IP = argv[1];
    port = atoi(argv[2]);

    sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock_fd == -1) {
		perror("Socket");
		exit(EXIT_FAILURE);
	}

	//Clear the sockaddr_un
	memset(&inet_addr, 0, sizeof(struct sockaddr_in));

	inet_addr.sin_family = AF_INET;
	inet_aton(MY_IP, (struct in_addr*) &inet_addr.sin_addr.s_addr);
	inet_addr.sin_port = htons(port);

	ret = bind(sock_fd, (const struct sockaddr*) &inet_addr, 
			sizeof(struct sockaddr_in));
	if (ret == -1) {
		perror("Binding");
		exit(EXIT_FAILURE);
	}

	ret = listen(sock_fd, 20);
	if (ret == -1) {
		perror("Listen");
		exit(EXIT_FAILURE);
	}

    printf("Connect to the server at %s:%d\n", MY_IP, port);

	peer_addr_size = sizeof(struct sockaddr_in);
	while(1) {
		data_fd = accept(sock_fd, (struct sockaddr*) &client_addr, 
				&peer_addr_size);
		if (data_fd == -1) {
			perror("Accept");
			exit(EXIT_FAILURE);
		}
		dprintf("Server: got a connection!\n");

		//Open a stream and read incoming messages until client disconnects
		while(1) {
            int pathlen, offset = 0;
            char* path;
            // Receive the path name from the client
            ret = read(data_fd, &pathlen, sizeof(int));
            if (ret == -1) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            path = malloc(pathlen * sizeof(char));
            while (offset < pathlen) {
                ret = read(data_fd, path + offset, pathlen - offset);
                if (ret == -1) {
                    perror("read");
                    exit(EXIT_FAILURE);
                }
                offset += ret;
            }
            // Receive the event type from the client
            ret = read(data_fd, &event_type, sizeof(enum f_event));
            if (ret == -1) {
                perror("read");
                exit(EXIT_FAILURE);
            }
            switch (event_type) {
                case f_create:
                    dprintf("Creating %s\n", path);
                    if (path[pathlen-2] == '/') { // New path is a directory
                        ret = mkdir(path, 0755);
                        if (ret == -1) {
                            perror("mkdir");
                            exit(EXIT_FAILURE);
                        }
                    } else { // New path is a file
                        ret = creat(path, 0755);
                        if (ret == -1) {
                            perror("creat");
                            exit(EXIT_FAILURE);
                        }
                        close(ret);
                    }
                    break;
                
                case f_delete:
                    dprintf("Deleting %s\n", path);
                    ret = remove(path);
                    if (ret == -1) {
                        perror("rmdir");
                        exit(EXIT_FAILURE);
                    }
                    break;

                case f_modify:
                    dprintf("Modifying %s\n", path);
                    off_t file_size;
                    int curr_file = open(path, O_WRONLY | O_TRUNC); 
                    if (curr_file == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    // Receive the file size, then contents
                    ret = read(data_fd, &file_size, sizeof(off_t));
                    if (ret == -1) {
                        perror("read");
                        exit(EXIT_FAILURE);
                    }
                    offset = 0;
                    while (offset < file_size) {
                        // Read a chunk of BUF_LEN bytes and write it out
                        int chunk_offset = 0;
                        while (chunk_offset < BUF_LEN && offset < file_size) {
                            ret = read(data_fd, read_buf + chunk_offset, 
                                    BUF_LEN - chunk_offset);
                            if (ret == -1) {
                                perror("read");
                                exit(EXIT_FAILURE);
                            }
                            chunk_offset += ret;
                            offset += ret;
                        }
                        ret = write(curr_file, read_buf, chunk_offset);
                        if (ret == -1) {
                            perror("write");
                            exit(EXIT_FAILURE);
                        }
                    }
                    break;
            }
        }
	} 
	return 0;
}	
