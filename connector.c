#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "connector.h"
#include "ae.h"
#include "mem.h"
#include "net.h"
#include "logging.h"

#define MAX_RECV_BUF_LEN    0x100000
#define MAX_SEND_Q_COUNT    128

#define MAX_CONNECTOR_SIZE  AE_SETSIZE

typedef struct send_q_t {
    void            *send_buf;  // 待发送数据
    int             buf_len;    // 数据总字节数
    int             send_len;   // 已发送字节数
    struct send_q_t *next;      // 队列链
} send_q_t;

typedef struct connector_t {
    struct timeval  created_at;
    struct timeval  updated_at;
    char        closed_flag;                // 被动关闭标记，不接收不发送，等待处理计数
    char        closing_flag;               // 主动关闭标记，不接收只发送，发送后清理
    char        ip[INET_ADDRSTRLEN];        // 客户端 IP
    int         port;                       // 客户端 PORT
    char        recv_buf[MAX_RECV_BUF_LEN]; // 数据接收缓冲区
    int         recv_len;                   // 已接收字节数
    send_q_t    send_q_head;                // 待发送数据包队列
    int         send_q_count;               // 待发送数据包个数
    int         processing_count;           // 处理中的数据包计数，非0时不清理
} connector_t;

connector_t *connector_ptr_array[MAX_CONNECTOR_SIZE] = {NULL};

// 设置连接主动关闭选项，不接收只发送
int connector_set_closing(int fd) {
    connector_t *con = connector_ptr_array[fd];
    con->closing_flag = 1;
    logging_trace("%s:%d %d set_closing", con->ip, con->port, fd);
    return 0;
}

// 初始化连接数据
int connector_init(int fd, const char *ip, int port) {
    connector_t *con = mem_malloc(sizeof(connector_t));
    gettimeofday(&(con->created_at), NULL);
    con->updated_at = con->created_at;
    con->closed_flag = 0;
    con->closing_flag = 0;
    memcpy(con->ip, ip, strlen(ip));
    con->port = port;
    con->recv_len = 0;
    con->send_q_head.next = NULL;
    con->send_q_count = 0;
    con->processing_count = 0;

    connector_ptr_array[fd] = con;
    logging_trace("%s:%d %d init", ip, port, fd);
    return 0;
}

// 清除该连接所有数据，不考虑各种计数，内部校验合法后调用
static int connector_destroy(int fd) {
    connector_t *con = connector_ptr_array[fd];
    close(fd);
    connector_ptr_array[fd] = NULL;
    send_q_t *q_ptr = con->send_q_head.next;
    while(q_ptr != NULL) {
        q_ptr = q_ptr->next;
        mem_free(q_ptr);
    }
    logging_trace("%s:%d %d destroy", con->ip, con->port, fd);
    mem_free(con);
    return 0;
}

int connector_get_info(int fd, char **ip, int *port, void **buf, int *len) {
    connector_t *con = connector_ptr_array[fd];
    *ip = con->ip;
    *port = con->port;
    *buf = con->recv_buf;
    *len = con->recv_len;
    logging_trace("%s:%d %d get_info recv_len %d", con->ip, con->port, fd, con->recv_len);
    return 0;
}

// 返回-1，未准备好可以处理的数据
int connector_pop_packet(int fd, void **data, int len) {
    connector_t *con = connector_ptr_array[fd];
    if(con->recv_len < len) {
        logging_trace("%s:%d %d pop_packet not ready", con->ip, con->port, fd);
        return -1;
    }
    *data = mem_malloc(len);
    memcpy(*data, con->recv_buf, len);
    if(con->recv_len > len) {
        memmove(con->recv_buf, (char *)(con->recv_buf) + len, con->recv_len - len);
    }
    con->recv_len -= len;

    // 连接引用计数
    con->processing_count ++;
    logging_trace("%s:%d %d pop_packet %d bytes, processing_count %d, recv_len %d", con->ip, con->port, fd, len, con->processing_count, con->recv_len);
    return 0;
}

// 返回-1，发送过慢异常，清空事件
// 返回 1，发送链刚刚建立，需要建立写事件
int connector_push_packet(int fd, void *data, int len) {
    connector_t *con = connector_ptr_array[fd];
    // 连接引用计数
    con->processing_count --;
    logging_trace("%s:%d %d push_packet %d bytes, processing_count %d", con->ip, con->port, fd, con->processing_count);
    if(len == 0) {
        return 0;
    }
    // 发送队列计数
    con->send_q_count ++;

    if(con->send_q_count > MAX_SEND_Q_COUNT) {
        logging_trace("%s:%d %d send_q_count %d overflow", con->ip, con->port, fd, con->send_q_count);
        con->closed_flag = 1;
        mem_free(data);
        con->send_q_count --;
        return -1;
    }

    logging_trace("%s:%d %d send_q_count %d", con->ip, con->port, fd, con->send_q_count);
    send_q_t *pre_ptr = &(con->send_q_head);
    while(pre_ptr->next != NULL) pre_ptr = pre_ptr->next;
    send_q_t *q_entry = (send_q_t *)mem_malloc(sizeof(send_q_t));
    q_entry->send_buf = data;
    q_entry->buf_len = len;
    q_entry->send_len = 0;
    q_entry->next = pre_ptr->next;
    pre_ptr->next = q_entry;
    return con->send_q_count;
}

// -1，连接超时异常，需要清空事件
int connector_check_one(int fd, struct timeval *tv, int idle_timeout_ms) {
    connector_t *con = connector_ptr_array[fd];
    if(con == NULL)
        return 0;
    if(con->closed_flag && con->processing_count == 0) {
        logging_trace("%s:%d %d check closed_flag processing_count 0", con->ip, con->port, fd);
        connector_destroy(fd);
        // 这里应该在之前清空过事件了，故不重复清除
        return 0;
    }
    else if(con->closing_flag && con->processing_count == 0 && con->send_q_count == 0) {
        logging_trace("%s:%d %d check closing_flag processing_count 0, send_q_count 0", con->ip, con->port, fd);
        connector_destroy(fd);
        // 这里需要清除可写事件，故返回-1
        return -1;
    }
    // 检查超时
    if(tv) {
        int milliseconds =
            1000 * (tv->tv_sec - con->updated_at.tv_sec) +
            (tv->tv_usec - con->updated_at.tv_usec) / 1000;
        if(milliseconds > idle_timeout_ms) {
            logging_trace("%s:%d %d check idle_timeout", con->ip, con->port, fd);
            con->closed_flag = 1;
            if(con->processing_count == 0)
                connector_destroy(fd);
            return -1;
        }
    }
    return 0;
}

// -1，连接状态异常，需要清空事件
int connector_recv(int fd) {
    connector_t *con = connector_ptr_array[fd];
    // 缓冲区满继续期望读，返回异常
    if(con->recv_len == MAX_RECV_BUF_LEN) {
        logging_trace("%s:%d %d recv_buf overflow", con->ip, con->port, fd);
        con->closed_flag = 1;
        if(con->processing_count == 0)
            connector_destroy(fd);
        return -1;
    }
    int num = net_read_n(fd, con->recv_buf + con->recv_len, MAX_RECV_BUF_LEN - con->recv_len);
    logging_trace("%s:%d %d read %d bytes", con->ip, con->port, fd, num);
    if(num == -1) {
        con->closed_flag = 1;
        if(con->processing_count == 0)
            connector_destroy(fd);
        return -1;
    }
    gettimeofday(&(con->updated_at), NULL);
    con->recv_len += num;
    logging_trace("%s:%d %d recv_len %d", con->ip, con->port, fd);
    return 0;
}
// 返回发送链元素数
// -1，连接状态异常，需要清空事件
//  0, 发送链空，需要清除写事件
int connector_send(int fd) {
    connector_t *con = connector_ptr_array[fd];
    while(1) {
        send_q_t *pre_ptr = &(con->send_q_head);
        send_q_t *s_ptr = pre_ptr->next;
        if(s_ptr == NULL)
            break;
        int num = net_write_n(fd, (char *)(s_ptr->send_buf) + s_ptr->send_len, s_ptr->buf_len - s_ptr->send_len);
        logging_trace("%s:%d %d write %d bytes", con->ip, con->port, fd, num);
        if(num == 0) {
            break;
        }
        else if(num == -1) {
            con->closed_flag = 1;
            if(con->processing_count == 0)
                connector_destroy(fd);
            return -1;
        }
        else {
            s_ptr->send_len += num;
            logging_trace("%s:%d %d send_buf_len %d, send_len %d", con->ip, con->port, fd, s_ptr->buf_len, s_ptr->send_len);
            if(s_ptr->send_len == s_ptr->buf_len) {
                pre_ptr->next = s_ptr->next;
                mem_free(s_ptr->send_buf);
                mem_free(s_ptr);
                // 发送队列计数
                con->send_q_count --;
                logging_trace("%s:%d %d send_q_count %d", con->ip, con->port, fd, con->send_q_count);
            }
        }
    }
    return con->send_q_count;
}
