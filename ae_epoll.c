#include "ae.h"
#include <sys/epoll.h>

typedef struct ae_api_state {
    int epfd;
    struct epoll_event events[AE_SETSIZE];
} ae_api_state;

static int ae_api_create(ae_event_loop *event_loop) {
    ae_api_state *state = mem_malloc(sizeof(ae_api_state));

    if(!state) return -1;
    state->epfd = epoll_create(1024);
    if(state->epfd == -1) return -1;
    event_loop->apidata = state;
    return 0;
}

static void ae_api_free(ae_event_loop *event_loop) {
    ae_api_state *state = event_loop->apidata;

    close(state->epfd);
    mem_free(state);
}

static int ae_api_add_event(ae_event_loop *event_loop, int fd, int mask) {
    ae_api_state *state = event_loop->apidata;
    struct epoll_event ev;

    int op = event_loop->events[fd].mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    
    ev.events = 0;
    mask |= event_loop->events[fd].mask;
    if(mask & AE_READABLE)
        ev.events |= EPOLLIN;
    if(mask & AE_WRITABLE)
        ev.events |= EPOLLOUT;
    ev.data.u64 = 0;
    ev.data.fd = fd;
    if(epoll_ctl(state->epfd, op, fd, &ev) == -1) return -1;
    return 0;
}

static void ae_api_del_event(ae_event_loop *event_loop, int fd, int delmask) {
    ae_api_state *state = event_loop->apidata;
    struct epoll_event ev;
    int op;
    int mask = event_loop->events[fd].mask & (~delmask);

    ev.events = 0;
    if(mask & AE_READABLE)
        ev.events |= EPOLLIN;
    if(mask & AE_WRITABLE)
        ev.events |= EPOLLOUT;
    ev.data.u64 = 0;
    ev.data.fd = fd;
    op = mask == AE_NONE ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
    
    epoll_ctl(state->epfd, op, fd, &ev);
}

static int ae_api_poll(ae_event_loop *event_loop, struct timeval *tvp) {
    ae_api_state *state = event_loop->apidata;
    int retval;
    int numevents = 0;

    retval = epoll_wait(state->epfd, state->events, AE_SETSIZE, tvp? (tvp->tv_sec*1000 + tvp->tv_usec/1000) : -1);
    if(retval > 0) {
        int j;
        numevents = retval;
        for(j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *ev = state->events + j;
            if(ev->events & EPOLLIN)
                mask |= AE_READABLE;
            if(ev->events & EPOLLOUT)
                mask |= AE_WRITABLE;
            event_loop->fired[j].fd = ev->data.fd;
            event_loop->fired[j].mask = mask;
        }
    }
    return numevents;
}

static char *ae_api_name() {
    return "epoll";
}
