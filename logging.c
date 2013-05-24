#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "logging.h"

static int logging_simple_flag = 1;
static logging_level_t logging_level = LOGGING_INFO;
static char *file_name = NULL;
static FILE *fp = NULL;

int logging_rotate() {
    if(fp) {
        fclose(fp);
        fp = fopen(file_name, "a");
        if(fp == NULL)
            return -1;
    }
    return 0;
}

static void Get_Now_Time(char* sTime)
{
    time_t tNow= time(NULL);
    struct tm tmNow;

    localtime_r(&tNow, &tmNow);

    //sprintf(sTime, "%04d-%02d-%02d %02d:%02d:%02d", (tmNow.tm_year+1900),
    //        tmNow.tm_mon+1, tmNow.tm_mday, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
    sprintf(sTime, "%02d/%02d/%04d:%02d:%02d:%02d", tmNow.tm_mday, tmNow.tm_mon+1,
            (tmNow.tm_year+1900), tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec);
}
int logging_init(const char *level, int simple_flag, const char *log_file) {
    if(strcasecmp(level, "FATAL") == 0)
        logging_level = LOGGING_FATAL;
    else if(strcasecmp(level, "ERROR") == 0)
        logging_level = LOGGING_ERROR;
    else if(strcasecmp(level, "WARN") == 0 || strcasecmp(level, "WARNING") == 0)
        logging_level = LOGGING_WARNING;
    else if(strcasecmp(level, "NOTICE") == 0)
        logging_level = LOGGING_NOTICE;
    else if(strcasecmp(level, "INFO") == 0)
        logging_level = LOGGING_INFO;
    else if(strcasecmp(level, "DEBUG") == 0)
        logging_level = LOGGING_DEBUG;
    else if(strcasecmp(level, "TRACE") == 0)
        logging_level = LOGGING_TRACE;

    logging_simple_flag = simple_flag;

    if(log_file) {
        fp = fopen(log_file, "a");

        if(fp == NULL)
            return -1;

        file_name = strdup(log_file);
        if(file_name == NULL)
            abort();
    }
    return 0;
}

void logging(logging_level_t level, const char *file, const char *func, int line, const char *fmt, ...) {
    char stime[50] = {};
    char buf[4096];
    va_list ap;
    int len = 0;

    if(level > logging_level)
        return;

    if(!logging_simple_flag)
        len = snprintf(buf, sizeof(buf), "%s:%d:in `%s`: ", file, line, func);

    if(sizeof(buf) > len) {
        va_start(ap, fmt);
        vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
        va_end(ap);
    }
    buf[sizeof(buf) - 1] = 0;

    Get_Now_Time(stime);

    const char *lvl;

    switch(level) {
        case LOGGING_FATAL:   lvl = "FATAL   "; break;
        case LOGGING_ERROR:   lvl = "ERROR   "; break;
        case LOGGING_WARNING: lvl = "WARNING "; break;
        case LOGGING_NOTICE:  lvl = "NOTICE  "; break;
        case LOGGING_INFO:    lvl = "INFO    "; break;
        case LOGGING_DEBUG:   lvl = "DEBUG   "; break;
        case LOGGING_TRACE:   lvl = "TRACE   "; break;
        default:              lvl = "UNKNOWN "; break;
    }

    if(fp)
        fprintf(fp, "[%s]%s %s\n", stime, lvl, buf);
    else if(level > LOGGING_WARNING)
        fprintf(stdout, "[%s]%s %s\n", stime, lvl, buf);
    else {
        fprintf(stderr, "[%s]%s %s\n", stime, lvl, buf);
    }
}

void boot_log(int failed_flag, const char *fmt, ...) {
    int i;
    int end;
    int pos;
    char log_buf[4096];
    va_list ap;

    va_start(ap, fmt);
    end = vsnprintf(log_buf, sizeof(log_buf), fmt, ap);
    va_end(ap);

    pos = 80 - 10 - end % 80;
    for(i = 0; i < pos; i++) {
        log_buf[end + i] = ' ';
    }
    log_buf[end + i] = '\0';
    strcat(log_buf, failed_flag == 0 ? "\e[1m\e[32m[ ok ]\e[m" : "\e[1m\e[31m[ failed ]\e[m");
    printf("\r%s\n", log_buf);

    if(failed_flag)
        exit(failed_flag);
}
