#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "msg_queue.h"
#include "mem.h"

#define DEFAULT_UNBOUNDED_QUEUE_SIZE 1024

struct msg_queue_t {
    volatile unsigned long long head;
    volatile unsigned long long tail;
    void **entries;
    int array_elements;
    int enable_lock;
    pthread_mutex_t mutex;
};

static int nextpow2(int num) {
    --num;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    return ++num;
}

int msg_queue_size(msg_queue_t *q) {
    int size;
    if(q->enable_lock)
        pthread_mutex_lock(&q->mutex);
    size = q->tail - q->head;
    if(q->enable_lock)
        pthread_mutex_unlock(&q->mutex);
    return size;
}

msg_queue_t *msg_queue_new(int size, int enable_lock) {
    struct msg_queue_t *q;
    q = (struct msg_queue_t *)mem_malloc(sizeof(*q));
    if(!size || !(q->array_elements = nextpow2(size)))
        q->array_elements = DEFAULT_UNBOUNDED_QUEUE_SIZE;

    q->entries = (void **)mem_malloc(sizeof(void *) * q->array_elements);
    q->head = q->tail = 0;

    if((q->enable_lock = enable_lock))
        pthread_mutex_init(&q->mutex, NULL);

    return q;
}
void msg_queue_destroy(msg_queue_t *q) {
    if(q) {
        if(q->entries)
            mem_free(q->entries);
        if(q->enable_lock)
            pthread_mutex_destroy(&q->mutex);
        mem_free(q);
    }
}

static int _msg_queue_push(msg_queue_t *q, void *data) {
    int idx;
    int rest = q->array_elements - (q->tail - q->head);
    if(rest == 0) {
        return -1;
    }
    idx = q->tail & (q->array_elements - 1);
    q->entries[idx] = data;
    q->tail ++;
    return (q->tail - q->head);
}

int msg_queue_push(msg_queue_t *q, void *data) {
    int ret;
    if(q->enable_lock)
        pthread_mutex_lock(&q->mutex);

    ret = _msg_queue_push(q, data);

    if(q->enable_lock)
        pthread_mutex_unlock(&q->mutex);
    return ret;
}

static void *_msg_queue_pop(msg_queue_t *q) {
    int idx;
    void *data;

    if(q->head == q->tail)
        return NULL;
    idx = q->head & (q->array_elements - 1);
    data = q->entries[idx];
    q->head ++;
    return data;
}

void *msg_queue_pop(msg_queue_t *q) {
    void *data;
    if(q->enable_lock)
        pthread_mutex_lock(&q->mutex);

    data = _msg_queue_pop(q);

    if(q->enable_lock)
        pthread_mutex_unlock(&q->mutex);
    return data;
}
