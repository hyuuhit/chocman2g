#ifndef _LOGGING_H_
#define _LOGGING_H_

typedef enum logging_level {
    LOGGING_FATAL,
    LOGGING_ERROR,
    LOGGING_WARNING,
    LOGGING_NOTICE,
    LOGGING_INFO,
    LOGGING_DEBUG,
    LOGGING_TRACE
} logging_level_t;

int logging_rotate();

int logging_init(const char *level, int simple_flag, const char *log_file);

void logging(logging_level_t level, const char *file, const char *func, int line, const char *fmt, ...);

#define logging_fatal(format, ...) \
        logging(LOGGING_FATAL, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)

#define logging_error(format, ...) \
        logging(LOGGING_ERROR, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)

#define logging_warn(format, ...) \
        logging(LOGGING_WARNING, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)

#define logging_notice(format, ...) \
        logging(LOGGING_NOTICE, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)

#define logging_info(format, ...) \
        logging(LOGGING_INFO, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)

#define logging_debug(format, ...) \
        logging(LOGGING_DEBUG, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)

#define logging_trace(format, ...) \
        logging(LOGGING_TRACE, __FILE__, __func__, __LINE__, format, ## __VA_ARGS__)

//#define BOOTLOG(faled_flag, args...) boot_log(faled_flag, args)
//void boot_log(int faled_flag, const char *fmt, ...);

#endif
