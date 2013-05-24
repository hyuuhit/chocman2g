#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "set_proc_title.h"
#include "mem.h"

extern char **environ;

int         saved_argc = 0;
char        **saved_argv = NULL;

static char *arg_start = NULL;
static char *arg_end = NULL;

void init_set_proc_titile(int argc, char **argv) {
    int i;

    if(arg_start)
        return;

    saved_argc = argc;
    saved_argv = mem_malloc(sizeof(char *) * argc);
    for(i = 0; i < argc; i++) {
        saved_argv[i] = mem_strdup(argv[i]);
    }
    arg_start = argv[0];
    arg_end = argv[argc-1] + strlen(argv[argc-1]);

    if(argc > 1)
        argv[1] = NULL;
}

static int save_environ() {
    static int environ_len = -1;
    char *env_start = arg_end + 1;
    int i;

    if(environ_len >= 0 || !arg_start)
        return environ_len;

    for(i = 0; environ[i]; i++) {
        if(arg_end + 1 == environ[i]) {
            arg_end = environ[i] + strlen(environ[i]);
            environ[i] = mem_strdup(environ[i]);
        }
        else {
            break;
        }
    }
    environ_len = arg_end - env_start + 1;
    return environ_len;
}

int set_proc_title(const char *fmt, ...) {
    char    title[64];
    int     title_len;
    int     space_len;
    va_list ap;

    if(!arg_start)
        return -1;

    va_start(ap, fmt);
    vsnprintf(title, sizeof(title), fmt, ap);
    va_end(ap);

    title_len = strlen(title) + 1;
    if(arg_end - arg_start + 1 < title_len)
        save_environ();

    space_len = arg_end - arg_start + 1;
    strncpy(arg_start, title, space_len);
    if(title_len > space_len)
        *arg_end = 0;

    return 0;
}

int set_proc_title_with_args(const char *fmt, ...) {
    char    *pos;
    char    title[64];
    int     title_len;
    int     env_len;
    int     i;
    va_list ap;

    if(!arg_start)
        return -1;

    va_start(ap, fmt);
    vsnprintf(title, sizeof(title), fmt, ap);
    va_end(ap);
    title_len = strlen(title);

    env_len = save_environ();
    if(env_len < title_len)
        return -1;

    strcpy(arg_start, title);
    pos = arg_start + strlen(title) + 1;
    *(pos - 1) = ' ';
    for(i = 0; i < saved_argc; i++) {
        strcpy(pos, saved_argv[i]);
        pos += strlen(saved_argv[i]) + 1;
        *(pos - 1) = ' ';
    }
    *(pos - 1) = 0;
    return 0;
}
