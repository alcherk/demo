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
#include	<assert.h>

#define RING_QUEUE_SIZE (40960)

struct send_node g_send_arr[RING_QUEUE_SIZE];

/* initialize all necessary variables for the queue */
int ring_queue_init(struct ring_queue *queue, int size)
{
	int rc_lock = pthread_mutex_init(&queue->lock, NULL);
	assert(rc_lock == 0);
	//int rc_cond = pthread_cond_init(&queue->cond, NULL);
	//assert(rc_cond == 0);
	queue->start = malloc(size * sizeof(void *));
	// exception handle
	int i = 0;	
	for (i = 0; i < RING_QUEUE_SIZE; i++) {
		queue->start[i] = (void *)&g_send_arr[i];
	}
	queue->capacity = size;
	queue->head = 0;
	queue->tail = 0;
	queue->validItem = 0;
	
	return 0;
}

/* push an item into the tail of the queue. If the queue is full, wait until someone pops an item */
int ring_queue_push(struct ring_queue *queue, void *item)
{
	pthread_mutex_lock(&queue->lock);
	// while full, wait
	while (queue->validItem >= queue->capacity) {
		//pthread_cond_wait(&queue->cond, &queue->lock);
		pthread_mutex_unlock(&queue->lock);
		return -1;
	}
	// push an item into the tail
	queue->validItem++;
	//queue->start[queue->tail] = item;
	memcpy(queue->start[queue->tail], item, sizeof(struct send_node));
	queue->tail = (queue->tail + 1) % queue->capacity;
	pthread_cond_broadcast(&queue->cond);
	pthread_mutex_unlock(&queue->lock);
	return 0;
}

/* pop an item from the top of the queue. If the queue is empty, wait until someone pushes an item */
void* ring_queue_pop(struct ring_queue *queue)
{
	void *item = NULL;

	pthread_mutex_lock(&queue->lock);
	// while empty, wait
	while (queue->validItem == 0) {
		//pthread_cond_wait(&queue->cond, &queue->lock);
		pthread_mutex_unlock(&queue->lock);
		return NULL;
	}
	// pop an item from the top
	item = queue->start[queue->head];
	queue->head = (queue->head + 1) % queue->capacity;
	queue->validItem--;
	//pthread_cond_broadcast(&queue->cond);
	pthread_mutex_unlock(&queue->lock);
	return item;
}

/* release all resources */
void ring_queue_destroy(struct ring_queue *queue)
{
	free(queue->start);
	pthread_mutex_destory(&queue->lock);
	// pthread_cond_destroy(&queue->cond);
}
