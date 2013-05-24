#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <poll.h>
#include <string.h>

#include "mem.h"

#include "ae.h"

#include "ae_epoll.c"


ae_event_loop *ae_create_event_loop() {
    ae_event_loop *event_loop;
    int i;
    event_loop = mem_malloc(sizeof(ae_event_loop));
    if(event_loop == NULL)
        abort();

    if(event_loop == NULL) return NULL;
    event_loop->time_event_head = NULL;
    event_loop->time_event_next_id = 0;
    event_loop->stop = 0;
    event_loop->maxfd = -1;
    event_loop->before_sleep = NULL;
    if(ae_api_create(event_loop) == -1) {
        mem_free(event_loop);
        return NULL;
    }
    for(i = 0; i < AE_SETSIZE; i++) {
        event_loop->events[i].mask = AE_NONE;
    }
    return event_loop;
}

void ae_delete_event_loop(ae_event_loop *event_loop) {
    ae_api_free(event_loop);
    mem_free(event_loop);
}

void ae_stop(ae_event_loop *event_loop) {
    event_loop->stop = 1;
}

int ae_create_file_event(ae_event_loop *event_loop, int fd, int mask, ae_file_proc *proc, void *client_data) {
    if(fd >= AE_SETSIZE)
        return AE_ERR;
    ae_file_event *fe = &event_loop->events[fd];

    if(ae_api_add_event(event_loop, fd, mask) == -1)
        return AE_ERR;
    fe->mask |= mask;
    if(mask & AE_READABLE)
        fe->r_file_proc = proc;
    if(mask & AE_WRITABLE)
        fe->w_file_proc = proc;
    fe->client_data = client_data;
    if(fd > event_loop->maxfd)
        event_loop->maxfd = fd;
    return AE_OK;
}

void ae_delete_file_event(ae_event_loop *event_loop, int fd, int mask) {
    if(fd >= AE_SETSIZE)
        return;
    ae_file_event *fe = &event_loop->events[fd];

    if(fe->mask == AE_NONE)
        return;
    fe->mask = fe->mask & (~mask);
    if(fd == event_loop->maxfd && fe->mask == AE_NONE) {
        int j;
        for(j = event_loop->maxfd - 1; j >= 0; j--) {
            if(event_loop->events[j].mask != AE_NONE)
                break;
        }
        event_loop->maxfd = j;
    }
    ae_api_del_event(event_loop, fd, mask);
}

int ae_get_file_events(ae_event_loop *event_loop, int fd) {
    if(fd >= AE_SETSIZE)
        return AE_NONE;
    ae_file_event *fe = &event_loop->events[fd];
    return fe->mask;
}

static void ae_get_time(long *seconds, long *millicseconds) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *seconds = tv.tv_sec;
    *millicseconds = tv.tv_usec/1000;
}

static void ae_add_milliseconds_to_now(long long milliseconds, long *sec, long *ms) {
    long cur_sec, cur_ms, when_sec, when_ms;
    ae_get_time(&cur_sec, &cur_ms);
    when_sec = cur_sec + milliseconds/1000;
    when_ms = cur_ms + milliseconds%1000;
    if(when_ms >= 1000) {
        when_sec ++;
        when_ms -= 1000;
    }
    *sec = when_sec;
    *ms = when_ms;
}

long long ae_create_time_event(ae_event_loop *event_loop, long long milliseconds,
        ae_time_proc *proc, void *client_data,
        ae_event_finalizer_proc *finalizer_proc) {
    long long id = event_loop->time_event_next_id++;
    ae_time_event *te;
    te = mem_malloc(sizeof(*te));
    if(te == NULL) abort();
    te->id = id;
    ae_add_milliseconds_to_now(milliseconds, &te->when_sec, &te->when_ms);
    te->time_proc = proc;
    te->finalizer_proc = finalizer_proc;
    te->client_data = client_data;
    te->next = event_loop->time_event_head;
    event_loop->time_event_head = te;
    return id;
}

int ae_delete_time_event(ae_event_loop *event_loop, long long id) {
    ae_time_event *te;
    ae_time_event *prev = NULL;
    te = event_loop->time_event_head;
    while(te) {
        if(te->id == id) {
            if(prev == NULL)
                event_loop->time_event_head = te->next;
            else
                prev->next = te->next;
            if(te->finalizer_proc)
                te->finalizer_proc(event_loop, te->client_data);
            mem_free(te);
            return AE_OK;
        }
        prev = te;
        te = te->next;
    }
    return AE_ERR;
}

static ae_time_event *ae_search_nearest_timer(ae_event_loop *event_loop) {
    ae_time_event *te = event_loop->time_event_head;
    ae_time_event *nearest = NULL;

    while(te) {
        if(!nearest || te->when_sec < nearest->when_sec ||
                (te->when_sec == nearest->when_sec && te->when_ms < nearest->when_ms))
            nearest = te;
        te = te->next;
    }
    return nearest;
}

static int process_time_events(ae_event_loop *event_loop) {
    int processed = 0;
    ae_time_event *te;
    long long maxid;

    te = event_loop->time_event_head;
    maxid = event_loop->time_event_next_id - 1;
    while(te) {
        long now_sec;
        long now_ms;
        long long id;

        if(te->id > maxid) {
            te = te->next;
            continue;
        }
        ae_get_time(&now_sec, &now_ms);
        if(now_sec > te->when_sec ||
                (now_sec == te->when_sec && now_ms >= te->when_ms)) {
            int retval;

            id = te->id;
            retval = te->time_proc(event_loop, id, te->client_data);
            processed++;
            if(retval != AE_NOMORE) {
                ae_add_milliseconds_to_now(retval, &te->when_sec, &te->when_ms);
            }
            else {
                ae_delete_time_event(event_loop, id);
            }
            te = event_loop->time_event_head;
        }
        else {
            te = te->next;
        }
    }
    return processed;
}

int ae_process_events(ae_event_loop *event_loop, int flags) {
    int processed = 0;
    int num_events;

    if(!(flags & AE_TIME_EVENTS) && !(flags & AE_FILE_EVENTS))
        return 0;
    if(event_loop->maxfd != -1 ||
            ((flags & AE_TIME_EVENTS) && !(flags & AE_DONT_WAIT))) {
        int j;
        ae_time_event *nearest = NULL;
        struct timeval tv;
        struct timeval *tvp;

        if(flags & AE_TIME_EVENTS && !(flags & AE_DONT_WAIT))
            nearest = ae_search_nearest_timer(event_loop);
        if(nearest) {
            long now_sec;
            long now_ms;

            ae_get_time(&now_sec, &now_ms);
            tvp = &tv;
            tvp->tv_sec = nearest->when_sec - now_sec;
            if(nearest->when_ms < now_ms) {
                tvp->tv_usec = ((nearest->when_ms + 1000) - now_ms) * 1000;
                tvp->tv_sec --;
            }
            else {
                tvp->tv_usec = (nearest->when_ms - now_ms) * 1000;
            }
            if(tvp->tv_sec < 0)
                tvp->tv_sec = 0;
            if(tvp->tv_usec < 0)
                tvp->tv_usec = 0;
        }
        else {
            if(flags & AE_DONT_WAIT) {
                tv.tv_sec = tv.tv_usec = 0;
                tvp = &tv;
            }
            else {
                tvp = NULL;
            }
        }

        num_events = ae_api_poll(event_loop, tvp);
        for(j = 0; j < num_events; j++) {
            int mask = event_loop->fired[j].mask;
            int fd = event_loop->fired[j].fd;
            int rfired = 0;
            ae_file_event *fe = &event_loop->events[fd];

            if(fe->mask & mask & AE_READABLE) {
                rfired = 1;
                fe->r_file_proc(event_loop, fd, fe->client_data, mask);
            }
            if(fe->mask & mask & AE_WRITABLE) {
                if(!rfired || fe->w_file_proc != fe->r_file_proc)
                    fe->w_file_proc(event_loop, fd, fe->client_data, mask);
            }
            processed ++;
        }
    }

    if(flags & AE_TIME_EVENTS)
        processed += process_time_events(event_loop);

    return processed;
}

int ae_wait(int fd, int mask, long long milliseconds) {
    struct pollfd pfd;
    int retmask = 0;
    int retval;

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = fd;
    if(mask & AE_READABLE)
        pfd.events |= POLLIN;
    if(mask & AE_WRITABLE)
        pfd.events |= POLLOUT;

    if((retval = poll(&pfd, 1, milliseconds)) == 1) {
        if(pfd.revents & POLLIN) retmask |= AE_READABLE;
        if(pfd.revents & POLLOUT) retmask |= AE_WRITABLE;
        if(pfd.revents & POLLERR) retmask |= AE_WRITABLE;
        if(pfd.revents & POLLHUP) retmask |= AE_WRITABLE;
        return retmask;
    }
    return retval;
}

void ae_main(ae_event_loop *event_loop) {
    event_loop->stop = 0;
    while(!event_loop->stop) {
        if(event_loop->before_sleep != NULL)
            event_loop->before_sleep(event_loop);
        ae_process_events(event_loop, AE_ALL_EVENTS);
    }
}

char *ae_get_api_name() {
    return ae_api_name();
}

void ae_set_before_sleep_proc(ae_event_loop *event_loop, ae_before_sleep_proc *before_sleep) {
    event_loop->before_sleep = before_sleep;
}


#ifdef _TEST_AE_

#include "net.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

char neterr[ERR_LEN];

static void listen_callback(ae_event_loop *event_loop, int fd, void *client_data, int mask) {
    char ip[INET_ADDRSTRLEN];
    int port;
    int newfd = net_tcp_accept(neterr, fd, ip, &port);
    if(newfd == NET_ERR) {
        printf("%s\n", neterr);
    }
    else {
        close(newfd);
        printf("ip: %s, port: %d\n", ip, port);
    }
}
int main() {
    ae_event_loop *el = ae_create_event_loop();
    if(el == NULL) return -1;

    int lfd = net_tcp_server(neterr, 9977, NULL);
    ae_create_file_event(el, lfd, AE_READABLE, listen_callback, NULL);
    ae_main(el);
    return 0;
}
#endif
