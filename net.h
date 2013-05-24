#ifndef _NET_H_

int net_tcp_server(const char *bind_ip, int port);
int net_tcp_accept(int fd, char *ip, int *port);
int net_set_nonblock(int fd);
int net_read_n(int fd, char *buf, int count);
int net_write_n(int fd, char *buf, int count);

#endif
