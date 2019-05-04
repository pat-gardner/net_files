//#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <netinet/ip.h>
#include <arpa/inet.h>

#include "my_socket.h"

#define MAX_ARGS 2

int main(int argc, char** argv) {
	int ret, data_socket;
	FILE* write_stream;
	char* msg = "This is a test message";
	char* dest_url = "128.252.167.161";
	struct sockaddr_in sock_addr;

	if (argc > MAX_ARGS) {
		perror("Too many args");
		return -1;
	}
	else if (argc == MAX_ARGS) {
		msg = argv[MAX_ARGS-1];
	}

	data_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (data_socket == -1) {
		perror("Socket");
		return -1;
	}

	//Set up the internet socket
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(SOCK_PORT);
	sock_addr.sin_addr.s_addr = inet_addr(dest_url);

	printf("Trying to connect\n");

	ret = connect(data_socket, (const struct sockaddr*) &sock_addr,
			sizeof(struct sockaddr_in));
	if (ret == -1) {
		perror("Connect");
		return -1;
	}

	printf("Connected to server\n");
	
	write_stream = fdopen(data_socket, "w");
	ret = fprintf(write_stream, "%s", msg);
	printf("Sent %d chars\n", ret);

	fclose(write_stream);
	return 0;
}
