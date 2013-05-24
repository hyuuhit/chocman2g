#ifndef _MSG_QUEUE_H_
#define _MSG_QUEUE_H_

typedef struct msg_queue_t msg_queue_t;

msg_queue_t *msg_queue_new(int size, int enable_lock);
void msg_queue_destroy(msg_queue_t *q);
int msg_queue_size(msg_queue_t *q);
int msg_queue_push(msg_queue_t *q, void *data);
void *msg_queue_pop(msg_queue_t *q);

#endif
