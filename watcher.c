#include <string.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>

#include "net_files.h"

#define IN_BUF_LEN 4096
#define MAX_FDS 1024 //Not portable, but this is the max on my raspi
#define NUM_ARGS 3

//TODO: create a directory for the fs root

/* Concatenate the file name child with the path parent.
 * Appends '/' to the end if child is a directory.
 * Returns a pointer that should be free'd when done.
 */
char* join_path(char* parent, char* child, int is_dir) {
    size_t pathlen = strlen(parent) + strlen(child) + 2;
    char* path = malloc(pathlen * sizeof(char));
    if (is_dir)
        snprintf(path, pathlen, "%s%s/", parent, child);
    else
        snprintf(path, pathlen, "%s%s", parent, child);
    return path;
}
/* Writes path to the file (probably a socket) represented
 * by fd. 
 */
void send_path(int fd, char* path) {
    int pathlen = strlen(path) + 1;
    int offset = 0, ret;
    // First send the length of the path
    ret = write(fd, &pathlen, sizeof(int));
    if (ret == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    // Then the actual pathname
    while (offset < pathlen) {
        ret = write(fd, path + offset, pathlen - offset);
        if (ret == -1) {
            perror("write");
            exit(EXIT_FAILURE);
        }
        offset += ret;
    }
}
/* Sends the contents of the file represented by path to 
 * the socket sock_fd
 */
void send_file(int sock_fd, char* path) {
    int ret, offset = 0;
    off_t file_size;
    struct stat stat_buf;
    int in_fd = open(path, O_RDONLY);
    // Send the file size, then contents 
    fstat(in_fd, &stat_buf);
    file_size = stat_buf.st_size;
    ret = write(sock_fd, &file_size, sizeof(off_t));
    if (ret == -1) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    while (offset < file_size) {
        ret = sendfile(sock_fd, in_fd, NULL, file_size);
        if (ret == -1) {
            perror("sendfile");
            exit(EXIT_FAILURE);
        }
        offset += ret;
    }
}

int main(int argc, char** argv) {
    int ifd, num_fds = 1;
    int in_my_flags = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | 
            IN_MOVED_FROM | IN_MOVED_TO; // TODO: IN_OPEN?
    int watch_fds[MAX_FDS];
    char buf[IN_BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    char* path_map[MAX_FDS]; // i'th index gives path of the watch descriptor i

    enum f_event event_type;

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

    if ((watch_fds[0] = inotify_add_watch(ifd, argv[1], in_my_flags)) == -1) {
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
        len = read(ifd, buf, IN_BUF_LEN);
        while (offset < len) {
            struct inotify_event *event = (struct inotify_event*) &buf[offset];
            dprintf("wd=%d, mask=%u, cookie=%u, len=%u, name=%s\n", event->wd, 
                    event->mask, event->cookie, event->len,
                    event->len > 0 ? event->name : "N/A");
            // Update offset to the index of the start of the next event
            offset += sizeof(struct inotify_event) + event->len;
            //Print the type of event
            if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) { 
                if (num_fds < MAX_FDS && event->len) {
                    // Add the new file to our local dictionary
                    char* path = join_path(path_map[event->wd], event->name, 
                            event->mask & IN_ISDIR);
                    int wfd;
                    dprintf("IN_CREATE: %s\n", path);
                    // Send the new file path to the server
                    send_path(sock_fd, path);
                    // Send the event type
                    event_type = f_create;
                    ret = write(sock_fd, &event_type, sizeof(enum f_event));
                    if (ret == -1) {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                    // If the new object is a regular file, send the contents
                    if ( !(event->mask & IN_ISDIR) )
                        send_file(sock_fd, path);
                    // Add the new file to the watch list
                    wfd = inotify_add_watch(ifd, path, in_my_flags);
                    if (wfd == -1) {
                        perror("Adding new watch");
                        return -1;
                    }
                    path_map[wfd] = path;
                    watch_fds[num_fds++] = wfd;
                }
            } 
            if (event->mask & IN_CLOSE_WRITE) {
                // If it's a file, send the contents
                if (event->len == 0) {
                    char* path = path_map[event->wd];
                    dprintf("IN_CLOSE_WRITE: %s\n", path);
                    // Send the pathname
                    send_path(sock_fd, path);
                    // Send the event type
                    event_type = f_modify;
                    ret = write(sock_fd, &event_type, sizeof(enum f_event));
                    if (ret == -1) {
                        perror("write");
                        exit(EXIT_FAILURE);
                    }
                    // Send the contents
                    send_file(sock_fd, path);
                }
            } 
            if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                // Send the path to the server
                char* path = join_path(path_map[event->wd], event->name, 
                        event->mask & IN_ISDIR);
                dprintf("IN_DELETE: %s\n", path);
                send_path(sock_fd, path);
                // Send the event type
                event_type = f_delete;
                ret = write(sock_fd, &event_type, sizeof(enum f_event));
                if (ret == -1) {
                    perror("write");
                    exit(EXIT_FAILURE);
                }
                // TODO: Remove the path from the path map
                //path_map[event->wd] = NULL;
                //TODO: remove from the list of FDs or remove list entirely
            }
            if (event->mask & IN_ACCESS) 
                dprintf("\tIN_ACCESS\n");
            if (event->mask & IN_ATTRIB)
                dprintf("\tIN_ATTRIB\n");
            if (event->mask & IN_CLOSE_NOWRITE)
                dprintf("\tIN_CLOSE_NOWRITE\n");
            if (event->mask & IN_MODIFY)
                dprintf("\tIN_MODIFY\n");
            if (event->mask & IN_MOVE_SELF)    
                dprintf("\tIN_MOVE_SELF\n");
            if (event->mask & IN_MOVED_FROM)
                dprintf("\tIN_MOVED_FROM\n");
            if (event->mask & IN_MOVED_TO)
                dprintf("\tIN_MOVED_TO\n");
            if (event->mask & IN_OPEN)
                dprintf("\tIN_OPEN\n");
        }
    }
    //TODO: free non-null entries in path_map
    return 0;
}
