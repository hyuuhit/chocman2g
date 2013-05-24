#ifndef _SET_PROC_TITLE_H_
#define _SET_PROC_TITLE_H_

extern int  saved_argc;
extern char **saved_argv;

void init_set_proc_titile(int argc, char **argv);

int set_proc_title(const char *fmt, ...);
int set_proc_title_with_args(const char *fmt, ...);

#endif
