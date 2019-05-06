#ifndef WATCHER_H
#define WATCHER_H

#include <stdio.h>

enum f_event{f_create, f_delete, f_modify};

// Only print debug info if we want to 
#define DEBUG 0
#define dprintf(...) if (DEBUG) {printf(__VA_ARGS__);}

#endif
