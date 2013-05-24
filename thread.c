#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/syscall.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thread.h"
#include "msg_queue.h"
#include "mem.h"

#define DEFAULT_WORKER_NUM 4

#define gettid() syscall(__NR_gettid)

typedef struct thread_t {
    int epfd;
    int notifyfd[2];
    int index;
    pthread_t id;
    msg_queue_t *queue;
    struct thread_pool_t *pool;
} thread_t;

struct thread_pool_t {
    pool_func func;
    void *pool_data;
    thread_t *threads;
    int num_threads;
    int queue_low_level;
    int heart_rate;
    pthread_mutex_t init_mutex;
    pthread_cond_t init_cond;
    volatile int init_count;
    volatile int stop;
    volatile int last_thread;
};

static int fd_set_nonblock(int fd) {
    int flags;
    if((flags = fcntl(fd, F_GETFL)) == -1)
        return -1;
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return -1;
    return 0;
}


static int setup_thread(thread_t *thread, int queue_size, int enable_lock) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;

    thread->epfd = epoll_create(10);
    if(thread->epfd == -1)
        return -1;
    if(-1 == pipe(thread->notifyfd)) {
        close(thread->epfd);
        return -1;
    }
    if(fd_set_nonblock(thread->notifyfd[0]) || fd_set_nonblock(thread->notifyfd[1])) {
        close(thread->epfd);
        close(thread->notifyfd[0]);
        close(thread->notifyfd[1]);
        return -1;
    }
    ev.data.fd = thread->notifyfd[0];
    if(epoll_ctl(thread->epfd, EPOLL_CTL_ADD, thread->notifyfd[0], &ev) == -1) {
        close(thread->epfd);
        close(thread->notifyfd[0]);
        close(thread->notifyfd[1]);
        return -1;
    }
    thread->queue = msg_queue_new(queue_size, enable_lock);
    if(thread->queue == NULL) {
        close(thread->epfd);
        close(thread->notifyfd[0]);
        close(thread->notifyfd[1]);
        return -1;
    }
    return 0;
}
static void unsetup_thread(thread_t *thread) {
    close(thread->epfd);
    close(thread->notifyfd[0]);
    close(thread->notifyfd[1]);
    msg_queue_destroy(thread->queue);
}

static void *thread_worker(void *data) {
    struct epoll_event ev;
    static char buf[64] = {};
    struct timeval last_active_time;
    struct timeval now_time;
    int intr_milliseconds;
    int sleep_milliseconds = 100;
    void *pop_data;
    thread_t *my = data;
    pthread_mutex_lock(&my->pool->init_mutex);
    my->index = my->pool->init_count ++;
    pthread_mutex_unlock(&my->pool->init_mutex);
    pthread_cond_broadcast(&my->pool->init_cond);

    gettimeofday(&last_active_time, NULL);

    while(!my->pool->stop || msg_queue_size(my->queue)) {
        if(my->pool->heart_rate) {
            gettimeofday(&now_time, NULL);
            intr_milliseconds = (now_time.tv_sec - last_active_time.tv_sec) * 1000 + (now_time.tv_usec - last_active_time.tv_usec) / 1000;
            sleep_milliseconds = my->pool->heart_rate - intr_milliseconds;
            if(sleep_milliseconds <= 0) {
                write(my->notifyfd[1], buf, 1);
                sleep_milliseconds = 0;
            }
        }

        if(epoll_wait(my->epfd, &ev, 1, sleep_milliseconds) > 0) {
            if(read(my->notifyfd[0], buf, sizeof(buf)) <= 0) {
                continue;
            }
            gettimeofday(&last_active_time, NULL);
            while((pop_data = msg_queue_pop(my->queue))) {
                my->pool->func(my->pool->pool_data, pop_data);
            }
        }
    }

    pthread_exit(NULL);
}

thread_pool_t *thread_pool_create(pool_func func, void *pool_data, int num_threads, int queue_low_level, int queue_hight_level, int heart_rate, int enable_lock) {
    int i;
    thread_pool_t *pool;
    pool = mem_malloc(sizeof(*pool));
    pool->func = func;
    pool->pool_data = pool_data;
    pool->num_threads = num_threads > 0 ? num_threads : DEFAULT_WORKER_NUM;
    pool->stop = 0;
    pool->init_count = 0;
    pool->last_thread = -1;
    pthread_mutex_init(&pool->init_mutex, NULL);
    pthread_cond_init(&pool->init_cond, NULL);

    if(queue_low_level > queue_hight_level)
        queue_low_level = queue_hight_level;
    else if(queue_low_level < 1) {
        queue_low_level = 1;
    }
    pool->queue_low_level = queue_low_level;

    if(heart_rate < 0) {
        heart_rate = 0;
    }
    pool->heart_rate = heart_rate;

    pool->threads = mem_malloc(sizeof(thread_t) * pool->num_threads);
    for(i = 0; i < pool->num_threads; i++) {
        pool->threads[i].pool = pool;
        if(setup_thread(&pool->threads[i], queue_hight_level, enable_lock) == -1) {
            i--;
            for(; i >= 0; i--) {
                unsetup_thread(&pool->threads[i]);
            }
            mem_free(pool->threads);
            mem_free(pool);
            return NULL;
        }
    }

    
    for(i = 0; i < pool->num_threads; i++) {
        if(pthread_create(&pool->threads[i].id, NULL, thread_worker, &pool->threads[i]) != 0) {
            // TODO 失败处理
        }
    }
    pthread_mutex_lock(&pool->init_mutex);
    while(pool->init_count < pool->num_threads) {
        pthread_cond_wait(&pool->init_cond, &pool->init_mutex);
    }
    pthread_mutex_unlock(&pool->init_mutex);
    return pool;
}

void thread_pool_destroy(thread_pool_t *pool) {
    int i;
    pool->stop = 1;
    for(i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i].id, NULL);
        unsetup_thread(&pool->threads[i]);
    }
    pthread_mutex_destroy(&pool->init_mutex);
    pthread_cond_destroy(&pool->init_cond);
    mem_free(pool->threads);
    mem_free(pool);
}

int thread_pool_dispatch(thread_pool_t *pool, void *data) {
    int index = (pool->last_thread + 1) % pool->num_threads;
    pool->last_thread = index;

    return thread_pool_dispatch_one(pool, data, index);
}

int thread_pool_dispatch_one(thread_pool_t *pool, void *data, int idx) {
    char buf[1] = {};
    thread_t *thread;
    int queue_size;
    if(idx >= pool->num_threads)
        return -1;
    thread = pool->threads + idx;
    queue_size = msg_queue_push(thread->queue, data);
    if(queue_size == pool->queue_low_level)
        write(thread->notifyfd[1], buf, 1);
    return queue_size;
}

void thread_pool_print(thread_pool_t *pool) {
    int i;
    printf("pool_data:   %p\n", pool->pool_data);
    printf("num_threads: %d\n", pool->num_threads);
    printf("low_level:   %d\n", pool->queue_low_level);
    printf("heart_rate:  %d\n", pool->heart_rate);
    for(i = 0; i < pool->num_threads; i++) {
        thread_t *thread = &pool->threads[i];
        printf("  %d worker thread\n", thread->index);
        printf("    epfd:       %d\n", thread->epfd);
        printf("    notifyfd:   %d, %d\n", thread->notifyfd[0], thread->notifyfd[1]);
        printf("    queue_size: %d\n", msg_queue_size(thread->queue));
    }
}
