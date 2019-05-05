#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/ip.h>
#include <arpa/inet.h>

#define BUF_SIZE 128
#define NUM_ARGS 2

int main(int argc, char** argv) {
	int sock_fd, data_fd, port, ret, done = 0;
	struct sockaddr_in inet_addr, client_addr;
	socklen_t peer_addr_size;
	FILE* read_stream;
	char read_buf[BUF_SIZE];
	char* MY_IP;
    
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
	while(1){
		data_fd = accept(sock_fd, (struct sockaddr*) &client_addr, 
				&peer_addr_size);
		if (data_fd == -1) {
			perror("Accept");
			exit(EXIT_FAILURE);
		}
		printf("Server: got a connection!\n");

		//Open a stream and read incoming messages until client disconnects
		while(1) {
            read_stream = fdopen(data_fd, "r");
            while ((ret = fscanf(read_stream, "%s", read_buf)) > 0) {
                if(strcmp(read_buf, "quit") == 0) {
                    printf("Quitting!\n");
                    done = 1;
                    break;
                }
                printf("%s\n", read_buf);
                sleep(1);
            }
            printf("Server: lost connection\n");
            close(data_fd);
        }
	} 

	return 0;
}	
