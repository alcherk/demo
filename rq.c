/*
 * =====================================================================================
 *
 *       Filename:  rq.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2017年06月26日 14时06分39秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Gordon Yang
 *        Company:  
 *
 * =====================================================================================
 */

#include	"rq.h"
#include	<pthread.h>
#include	<stdio.h>
#include	<stdlib.h>
#include 	<emmintrin.h>

#define RING_QUEUE_SIZE (40960)

struct send_node g_send_arr[RING_QUEUE_SIZE];

/* initialize all necessary variables for the queue */
int ring_queue_init(struct ring_queue *queue, int size)
{
	int i = 0;	
	queue->start = malloc(size * sizeof(void *));
	if (queue->start == NULL)
		return -1;
	for (i = 0; i < RING_QUEUE_SIZE; i++) {
		queue->start[i] = (void *)&g_send_arr[i];
	}
	queue->w_lock = 0;
	queue->r_lock = 0;
	queue->capacity = size;
	queue->head = 0;
	queue->tail = 0;
	queue->read_cnt = 0;
	queue->write_cnt = 0;
	
	return 0;
}

void ring_queue_change_node_status(struct send_node *p, rq_node_status_t s)
{
	ATOMIC_SET(p->status, s);
}

void* ring_queue_push(struct ring_queue *queue)
{
	void *item = NULL;
	struct send_node *p = NULL;
	while (__sync_lock_test_and_set(&queue->w_lock, 1))
		//_mm_pause();
		usleep(50);
	while (queue->write_cnt - queue->read_cnt >= queue->capacity) {
		__sync_lock_release(&queue->w_lock);
		return NULL;
	}
	// push an item into the tail
	p = item = queue->start[queue->tail];
	if (ATOMIC_READ(p->status) == RQUEUE_NODE_WRITED) {
		__sync_lock_release(&queue->w_lock);
		return NULL;
	}
	mb();
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->write_cnt++;
	mb();
	__sync_lock_release(&queue->w_lock);
	return item;
}

void* ring_queue_pop(struct ring_queue *queue)
{
	void *item = NULL;
	struct send_node *p = NULL;

	while (__sync_lock_test_and_set(&queue->r_lock, 1))
		//_mm_pause();
		usleep(50);
	while (queue->read_cnt == queue->write_cnt) {
		__sync_lock_release(&queue->r_lock);
		return NULL;
	}
	// pop an item from the top
	p = item = queue->start[queue->head];

	if (ATOMIC_READ(p->status) != RQUEUE_NODE_WRITED) {
		__sync_lock_release(&queue->r_lock);
		return NULL;
	} else { 
		mb();
		queue->head = (queue->head + 1) % queue->capacity;
		queue->read_cnt++;
		mb();
		__sync_lock_release(&queue->r_lock);
		return item;
	}
}

/* release all resources */
void ring_queue_destroy(struct ring_queue *queue)
{
	free(queue->start);
}
