#include <stdio.h>
#include <stdlib.h>

int handle_init(int argc, const char **argv);

int handle_open(void **send_buf, int *send_length, const char *ip, int port);

int handle_input(const void *buf, int length, const char *ip, int port);

int handle_process(const void *recv_buf, int recv_length, void **send_buf, int *send_length, const char *ip, int port);
