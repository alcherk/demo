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

#define RING_QUEUE_SIZE (40960)

struct send_node g_send_arr[RING_QUEUE_SIZE];

/* initialize all necessary variables for the queue */
int ring_queue_init(struct ring_queue *queue, int size)
{
	int i = 0;	
	int rc_lock = pthread_mutex_init(&queue->w_lock, NULL);
	if (rc_lock < 0)
		return -1;
	rc_lock = pthread_mutex_init(&queue->r_lock, NULL);
	if (rc_lock < 0)
		return -1;
	queue->start = malloc(size * sizeof(void *));
	if (queue->start == NULL)
		return -1;
	for (i = 0; i < RING_QUEUE_SIZE; i++) {
		queue->start[i] = (void *)&g_send_arr[i];
	}
	queue->capacity = size;
	queue->head = 0;
	queue->tail = 0;
	queue->read_cnt = 0;
	queue->write_cnt = 0;
	
	return 0;
}

int ring_queue_push(struct ring_queue *queue, void *item)
{
	while(pthread_mutex_lock(&queue->w_lock) < 0)
		usleep(50);
	// while full, wait
	while (queue->write_cnt - queue->read_cnt >= queue->capacity) {
		pthread_mutex_unlock(&queue->w_lock);
		//pthread_mutex_unlock(&queue->r_lock);
		return -1;
	}
	// push an item into the tail
	memcpy(queue->start[queue->tail], item, sizeof(struct send_node));
	barrier();	
	queue->tail = (queue->tail + 1) % queue->capacity;
	queue->write_cnt++;
	pthread_mutex_unlock(&queue->w_lock);
	return 0;
}

void* ring_queue_pop(struct ring_queue *queue)
{
	void *item = NULL;

	while(pthread_mutex_lock(&queue->r_lock) < 0)
		usleep(50);
	// while empty, wait
	while (queue->read_cnt - queue->write_cnt == 0) {
		//pthread_mutex_unlock(&queue->w_lock);
		pthread_mutex_unlock(&queue->r_lock);
		return NULL;
	}
	// pop an item from the top
	item = queue->start[queue->head];
	barrier();
	queue->head = (queue->head + 1) % queue->capacity;
	queue->read_cnt++;
	pthread_mutex_unlock(&queue->r_lock);
	return item;
}

/* release all resources */
void ring_queue_destroy(struct ring_queue *queue)
{
	free(queue->start);
}
