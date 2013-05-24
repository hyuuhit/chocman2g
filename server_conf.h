#ifndef _SERVER_CONF_H_
#define _SERVER_CONF_H_

typedef struct server_t {
    int         daemon;
    const char  *bind_ip;
    int         bind_port;
    int         idle_timeout;
    int         q_level;
    int         q_length;
    int         q_rate;
    int         worker_num;
    const char  *log_file;
    const char  *log_level;
    int         log_simple_flag;
} server_t;

extern server_t server;

int load_server_config(const char *config_file);
const char *server_config_get_str(const char *key);
int server_config_get_int(const char *key, int default_value);

#endif
