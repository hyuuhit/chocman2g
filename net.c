#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "logging.h"

int net_set_nonblock(int fd) {
    int flags;
    if((flags = fcntl(fd, F_GETFL)) == -1) {
        logging_error("fcntl(%d, F_GETFL) failed: %s", fd, strerror(errno));
        return -1;
    }
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        logging_error("fcntl(%d, F_SETFL, O_NONBLOCK) failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static int net_create_tcp_socket() {
    int fd;
    int on = 1;
    if((fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        logging_error("socket failed: %s", strerror(errno));
        return -1;
    }
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
        logging_error("setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

int net_tcp_server(const char *bind_ip, int port) {
    int fd;
    struct sockaddr_in addr;
    if((fd = net_create_tcp_socket()) == -1)
        return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind_ip && inet_aton(bind_ip, &addr.sin_addr) == 0) {
        logging_error("\"%s\" invalid bind ip", bind_ip);
        close(fd);
        return -1;
    }
    if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        logging_error("\"%s:%d\" bind failed: %s", bind_ip, port, strerror(errno));
        close(fd);
        return -1;
    }
    if(listen(fd, 511) == -1) {
        logging_error("\"%s:%d\" listen failed: %s", bind_ip, port, strerror(errno));
        close(fd);
        return -1;
    }
    return fd;
}

int net_tcp_accept(int fd, char *ip, int *port) {
    int newfd;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if((newfd = accept(fd, (struct sockaddr *)&addr, &len)) == -1) {
        logging_error("accept failed: %s", strerror(errno));
        return -1;
    }
    if(ip)
        inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
    if(port)
        *port = ntohs(addr.sin_port);
    return newfd;
}

int net_read_n(int fd, char *buf, int count) {
    int nread;
    int total = 0;
    if(count == 0)
        return 0;
    while(total != count) {
        nread = read(fd, buf, count - total);
        if(nread == 0) {
            logging_trace("fd %d closed by peer", fd);
            return -1;
        }
        if(nread == -1) {
            if(errno == EINTR)
                nread = 0;
            else if(errno == EAGAIN)
                break;
            else {
                logging_trace("fd %d read failed: %s", fd, strerror(errno));
                return -1;
            }
        }
        total += nread;
        buf += nread;
    }
    logging_trace("fd %d read %d bytes", fd, total);
    return total;
}

int net_write_n(int fd, char *buf, int count) {
    int nwrite;
    int total = 0;
    if(count == 0)
        return 0;
    while(total != count) {
        nwrite = write(fd, buf, count - total);
        if(nwrite == 0) {
            logging_trace("fd %d closed by peer", fd);
            return -1;
        }
        if(nwrite == -1) {
            if(errno == EINTR)
                nwrite = 0;
            else if(errno == EAGAIN)
                break;
            else {
                logging_trace("fd %d write failed: %s", fd, strerror(errno));
                return -1;
            }
        }
        total += nwrite;
        buf += nwrite;
    }
    logging_trace("fd %d write %d bytes", fd, total);
    return total;
}
