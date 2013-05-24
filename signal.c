#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <ucontext.h>

#include "signal.h"
#include "log.h"
#include "server_conf.h"

static void sigterm_handler(int sig) {
    server.stop = 1;
}

static void sigsegv_handler(int sig, siginfo_t *info, void *secret) {
    void *trace[100];
    char **messages = NULL;
    int i;
    int trace_size = 0;
    ucontext_t *uc = (ucontext_t *)secret;
    struct sigaction act;

    logging_fatal("=== START ===\n");
    logging_fatal("crashed by signal: %d\n", sig);

    trace_size = backtrace(trace, 100);

    trace[1] = (void *)uc->uc_mcontext.gregs[16];

    messages = backtrace_symbols(trace, trace_size);
    logging_fatal("--- STACK TRACE\n");
    for(i = 1; i < trace_size; i++) {
        logging_fatal("%s\n", messages[i]);
    }

    logging_fatal("=== END ===\n");

    // 保险起见。（可以取消）
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND;
    act.sa_handler = SIG_DFL;
    sigaction(sig, &act, NULL);

    kill(getpid(), sig);
}

static void sigreload_handler(int sig) {
    server.reload = 1;
}

void setup_signal_handler() {
    struct sigaction act;

    // 忽略两个信号
    signal(SIGPIPE, SIG_IGN);

    // 捕获term信号，kill命令默认信号
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND; 
    act.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &act, NULL);

    // 捕获HUP信号，reload
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND; 
    act.sa_handler = sigreload_handler;
    sigaction(SIGHUP, &act, NULL);

    // 捕获产生coredump信号
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_ONSTACK | SA_RESETHAND | SA_SIGINFO;
    act.sa_sigaction = sigsegv_handler;
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGFPE, &act, NULL);
    sigaction(SIGILL, &act, NULL);
    sigaction(SIGBUS, &act, NULL);
}
