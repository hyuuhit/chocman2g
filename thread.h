#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_

typedef void (*pool_func)(void *pool_data, void *packet_data);
typedef struct thread_pool_t thread_pool_t;


// 回调指针，回调参数0，线程数，低水平触发点，队列长度，最大心跳时间，队列操作是否加锁
struct thread_pool_t *thread_pool_create(pool_func func, void *pool_data, int num_threads, int queue_low_level, int queue_hight_level, int heart_rate, int enable_lock);
void thread_pool_destroy(struct thread_pool_t *pool);
void thread_pool_print(struct thread_pool_t *pool);
int thread_pool_dispatch(thread_pool_t *pool, void *data);
int thread_pool_dispatch_one(thread_pool_t *pool, void *data, int idx);

#endif
