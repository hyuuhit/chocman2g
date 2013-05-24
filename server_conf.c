#include <stdio.h>
#include <stdlib.h>

#include "server_conf.h"
#include "logging.h"
#include "setting.h"

server_t server;
static setting_t *server_setting = NULL;

static void init_server_config() {
    server.daemon = 0;
    server.bind_ip = NULL;
    server.bind_port = -1;
    server.idle_timeout = 60;
    server.q_level = 50;
    server.q_length = 10000;
    server.worker_num = 4;
    server.log_file = NULL;
    server.log_level = NULL;
    server.log_simple_flag = 1;
}

int load_server_config(const char *file) {
    init_server_config();
    if(file == NULL) {
        logging_trace("running with default setting");
    }
    else {
        server_setting = setting_create(file);
        if(server_setting == NULL) {
            logging_error("load config file \"%s\" failed", file);
            return -1;
        }
        else {
            logging_trace("load config file \"%s\"", file);
        }
    }

    const char *s;
    if((s = setting_get_str(server_setting, "bind_ip")))
        server.bind_ip = s;
    if((s = setting_get_str(server_setting, "log_file")))
        server.log_file= s;
    if((s = setting_get_str(server_setting, "log_level")))
        server.log_level= s;

    server.daemon           = setting_get_int(server_setting, "daemon", server.daemon);
    server.bind_port        = setting_get_int(server_setting, "bind_port", server.bind_port);
    server.idle_timeout     = setting_get_int(server_setting, "idle_timeout", server.idle_timeout);
    server.q_level          = setting_get_int(server_setting, "q_level", server.q_level);
    server.q_length         = setting_get_int(server_setting, "q_length", server.q_length);
    server.q_rate           = setting_get_int(server_setting, "q_rate", server.q_rate);
    server.worker_num       = setting_get_int(server_setting, "worker_num ", server.worker_num);
    server.log_simple_flag  = setting_get_int(server_setting, "log_simple_flag", server.log_simple_flag);

    if(server.bind_port == -1) {
        logging_error("not valid bind_port");
        setting_destroy(server_setting);
        return -1;
    }
    return 0;
}

const char *server_config_get_str(const char *key) {
    return setting_get_str(server_setting, key);
}
int server_config_get_int(const char *key, int default_value) {
    return setting_get_int(server_setting, key, default_value);
}
