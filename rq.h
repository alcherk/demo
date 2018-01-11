/*
 * =====================================================================================
 *
 *       Filename:  rq.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017年06月26日 11时43分58秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Gordon Yang
 *        Company:  
 *
 * =====================================================================================
 */
#ifndef _BUFFERED_QUEUE_H
#define	_BUFFERED_QUEUE_H

#include	<pthread.h>

#define ATOMIC_READ(_v) __sync_fetch_and_add(&(_v), 0)
#define ATOMIC_CAS(_v, _o, _n) __sync_bool_compare_and_swap(&(_v), (_o), (_n))
#define ATOMIC_SET(_v, _n) {\
    int _b = 0;\
    do {\
        _b = ATOMIC_CAS(_v, ATOMIC_READ(_v), _n);\
    } while (__builtin_expect(!_b, 0));\
}

typedef enum {
    RQUEUE_NODE_INIT = 0,
    RQUEUE_NODE_READED,
    RQUEUE_NODE_WRITED
} rq_node_status_t;


struct send_node {
	rq_node_status_t status;	
    int fd;
    unsigned int dlen;
    char    data[256];
};

struct ring_queue {
    /* define all necessary variables here */
    //pthread_mutex_t w_lock;
    //pthread_mutex_t r_lock;
	int w_lock;
	int r_lock;
    void **start;
    int capacity;
    int head;
    int tail;
    int write_cnt;
    int read_cnt;
	int validItem;
};

#define mb() __asm__ __volatile__("mfence":::"memory")

int ring_queue_init(struct ring_queue *queue, int size);
void ring_queue_change_node_status(struct send_node *p, rq_node_status_t s);
void* ring_queue_push(struct ring_queue *queue);
void* ring_queue_pop(struct ring_queue *queue);
void ring_queue_destroy(struct ring_queue *queue);

#endif
