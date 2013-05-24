#include <stdio.h>
#include <stdlib.h>

int handle_open(void **send_buf, int *send_length, const char *ip, int port) {
    printf("handle_open %s:%d\n", ip, port);
    return -1;
}
