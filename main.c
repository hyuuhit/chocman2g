#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "thread.h"
#include "logging.h"
#include "set_proc_title.h"
#include "server_conf.h"
#include "net.h"
#include "api.h"
#include "ae.h"
#include "connector.h"

int sofd;
ae_event_loop *el;

static void show_usage() {
    // TODO
    printf("usage: TODO\n");
}

static void show_version() {
    // TODO
    printf("version: TODO\n");
}

static int timer_callback(struct ae_event_loop *el, long long fd, void *no_use) {
    // TODO
    logging_trace("timer triggered");
    return 1000;
}
static void listen_callback(struct ae_event_loop *el, int fd, void *client_data, int mask) {
    // TODO
    char    ip[INET_ADDRSTRLEN];
    int     port;
    int     newfd;
    int     retval;
    void    *send_buf;
    int     send_length;

    newfd = net_tcp_accept(fd, ip, &port);
    if(fd >= 0) {
        logging_trace("accept fd %d, from %s:%d", newfd, ip, port);
    }
    retval = handle_open(&send_buf, &send_length, ip, port);
}

int setup_master() {
    // TODO
    sofd = net_tcp_server(server.bind_ip, server.bind_port);
    if(sofd == -1)
        return -1;

    el = ae_create_event_loop();
    if(el == NULL) {
        logging_error("ae_create_event_loop failed");
        close(sofd);
        return -1;
    }
    if(ae_create_file_event(el, sofd, AE_READABLE, listen_callback, NULL) == AE_ERR) {
        logging_error("ae_create_file_event for sofd: %d failed", sofd);
        close(sofd);
        ae_delete_event_loop(el);
        return -1;
    }
    return 0;
}

//int setup_worker() {
//    // TODO
//}

int main(int argc, char **argv) {
    const char *config_file = NULL;
    // 设置错误输出无缓冲
    setbuf(stderr, NULL);

    // 保存启动参数，为修改进程标题做准备
    init_set_proc_titile(argc, argv);

#define NA no_argument
#define RA required_argument
#define OA optional_argument
    int current_config = 0;
    while(1) {
        int c;
        static struct option cmd_options[] = {
            {"help", NA, NULL, 'h'},
            {"version", NA, NULL, 'v'},
            {"current-config", NA, NULL, 'V'},
            {"config-file", RA, NULL, 'c'},
            {0, 0, 0, 0}
        };
        c = getopt_long(saved_argc, saved_argv, "hvVc:", cmd_options, NULL);
        if(c == -1)
            break;
        switch(c) {
            case '?':
                exit(1);
            case 'h':
                show_usage();
                exit(1);
            case 'v':
                show_version();
                exit(1);
            case 'V':
                current_config = 1;
                break;
            case 'c':
                config_file = optarg;
                break;
        }
    }
    optind = 0;
    optarg = NULL;
    // 初始化（加载）配置 TODO
    if(config_file == NULL) {
        logging_error("need config file");
        exit(1);
    }
    else if(load_server_config(config_file) != 0) {
        exit(1);
    }
    if(current_config) {
        show_version();
        printf("TODO\n");
        exit(0);
    }

    if(setup_master() != 0) {
        exit(1);
    }
    logging_init("trace", 1, NULL);
    ae_create_time_event(el, 1000, timer_callback, NULL, NULL);
    ae_main(el);

    exit(0);
}
