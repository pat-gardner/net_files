#include <string.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define BUF_LEN 4096
#define MAX_FDS 128
#define NUM_ARGS 3

int main(int argc, char** argv) {
    int ifd, num_fds = 1;
    int watch_fds[MAX_FDS];
    char buf[BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    char* path_map[1024]; // i'th index gives path of the watch descriptor i

    int ret, port, sock_fd;
    char* server_ip;
    struct sockaddr_in sock_addr;

    if (argc != NUM_ARGS + 1) {
        printf("Usage: %s <path/to/watch> <server IP> <server port>\n", argv[0]);
        return -1;
    }
    server_ip = argv[2];
    port = atoi(argv[3]);

    // Create the inotify instance and watch the root directory
    if ((ifd = inotify_init()) == -1) {
        perror("Creating inotify");
        return -1;
    }

    if ((watch_fds[0] = inotify_add_watch(ifd, argv[1], IN_ALL_EVENTS)) == -1) {
        perror("Add watch");
        return -1;
    }
    printf("Added watch for %s\n", argv[1]);

    // Initialize the socket connection
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    inet_aton(server_ip, &sock_addr.sin_addr);

    printf("Trying to connect to server at %s:%d\n", server_ip, port);
    ret = connect(sock_fd, (struct sockaddr*) &sock_addr, 
            sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    // Initialize the map and the first path
    memset(path_map, 0, sizeof(path_map));
    path_map[watch_fds[0]] = argv[1];

    while (1) {
        ssize_t len, offset = 0;
        len = read(ifd, buf, BUF_LEN);
        while (offset < len) {
            struct inotify_event *event = (struct inotify_event*) &buf[offset];
            printf("wd=%d, mask=%u, cookie=%u, len=%u, name=%s\n", event->wd, 
                    event->mask, event->cookie, event->len,
                    event->len > 0 ? event->name : "N/A");
            // Update offset to the index of the start of the next event
            offset += sizeof(struct inotify_event) + event->len;
            //Print the type of event
            if (event->mask & IN_CREATE) {
                printf("\tIN_CREATE\n");
                if (num_fds < MAX_FDS && event->len) {
                    size_t pathlen = strlen(path_map[event->wd]) + event->len + 1;
                    char* path = (char*) malloc(pathlen * sizeof(char));
                    int wfd;
                    if (event->mask & IN_ISDIR) 
                        snprintf(path, pathlen, "%s%s/", path_map[event->wd], event->name);
                    else
                        snprintf(path, pathlen, "%s%s", path_map[event->wd], event->name);
                    printf("Trying to add: %s\n", path);
                    wfd = inotify_add_watch(ifd, path, IN_ALL_EVENTS);
                    if (wfd == -1) {
                        perror("Adding new watch");
                        return -1;
                    }
                    path_map[wfd] = path;
                    watch_fds[num_fds++] = wfd;
                }
            }
            if (event->mask & IN_ACCESS) 
                printf("\tIN_ACCESS\n");
            if (event->mask & IN_ATTRIB)
                printf("\tIN_ATTRIB\n");
            if (event->mask & IN_CLOSE_WRITE)
                printf("\tIN_CLOSE_WRITE\n");
            if (event->mask & IN_CLOSE_NOWRITE)
                printf("\tIN_CLOSE_NOWRITE\n");
            if (event->mask & IN_DELETE)
                printf("\tIN_DELETE\n");
            if (event->mask & IN_DELETE_SELF)
                printf("\tIN_DELETE_SELF\n");
            if (event->mask & IN_MODIFY)
                printf("\tIN_MODIFY\n");
            if (event->mask & IN_MOVE_SELF)    
                printf("\tIN_MOVE_SELF\n");
            if (event->mask & IN_MOVED_FROM)
                printf("\tIN_MOVED_FROM\n");
            if (event->mask & IN_MOVED_TO)
                printf("\tIN_MOVED_TO\n");
            if (event->mask & IN_OPEN)
                printf("\tIN_OPEN\n");
        }
    }
    
    return 0;
}
