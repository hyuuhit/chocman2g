#ifndef _CONNECTOR_H_
#define _CONNECTOR_H_


void    connector_set_closing(int fd);
int     connector_init(int fd, const char *ip, int port);
int     connector_recv(int fd);
int     connector_send(int fd);
int     connector_check_all(int idle_timeout);
void    connector_get_info(int fd, char **ip, int *port, void **buf, int *len);
int     connector_pop_packet(int fd, void **data, int len);
int     connector_push_packet(int fd, void *data, int len);
int     connector_check_one(int fd, struct timeval *tv, int idle_timeout_ms);

#endif
