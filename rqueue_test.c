#define _GNU_SOURCE
#include <sched.h>

#include "rq.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>


#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define CLOCKID		 CLOCK_MONOTONIC 
#define SIG SIGRTMIN

#define SIZE_OF_BUFFER 512
#define NUM_OF_WRITER 5
#define NUM 1000
#define MAX_WORKER 16
#define MAX_FILLER MAX_WORKER

int th_num[MAX_WORKER] = {0};
static struct ring_queue ring;
static int end;
static int write_count = 0;
static int read_count = 0;
static int allcnt = 1;
int retries = 0;

struct	ring_queue g_rq;


void set_thread_affinity(int pthread_num)
{
    int i = 0, cpu_cores;
    cpu_set_t mask;
    cpu_set_t get;

    cpu_cores = sysconf(_SC_NPROCESSORS_CONF);

    // i = cpu_cores - pthread_num - 1;
    //HE_LOG("g_he_core_bind_start [%d]", g_he_core_bind_start);
    i = pthread_num;
    CPU_ZERO(&mask);
    CPU_SET(i, &mask);

    // printf("Set thread affinity pthread_id is %d cpu is %d\n", pthread_num, i);
    if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
        // printf("Set thread affinity failed, pthread_id is %d\n", pthread_num);
        return;
    }

    CPU_ZERO(&get);
    if (pthread_getaffinity_np(pthread_self(), sizeof(get), &get) < 0) {
        //printf("Get thread affinity failed, pthread_id is %d\n", pthread_num);
    }
    // printf("--Thread %d is running in processor %d\n", pthread_num, CPU_COUNT(&get));
    if (CPU_ISSET(i, &get)) {
        // printf("Thread %d is running in processor %d\n", pthread_num, i);
    }
    return;
}


void *write_thread(void *ptr) {
    int i;
	struct send_node *p;
	void *item = NULL;
    for (i = 0; i < NUM; i++) {
		item = ring_queue_push(&ring);
		
		if (!item) {
			//
			p = item;		
			p->dlen = write_count;
			__sync_fetch_and_add(&write_count, 1);
        }
    }
    return NULL;
}

void *read_thread(void *ptr) {
    while(1) {
        if(__sync_fetch_and_add(&end, 0))
            break;

		void* val = ring_queue_pop(&ring);
        if(val) 
			__sync_fetch_and_add(&read_count, 1);
    }
    return NULL;
}

void test_multiple_writers_one_reader() {
    printf("Test multiple writers and one reader\n");
	memset(&ring, 0, sizeof(struct ring_queue));
	ring_queue_init(&ring, 40960);
    pthread_t writer[NUM_OF_WRITER];
    pthread_t reader;
    end = 0;
    pthread_create(&reader, NULL, read_thread, NULL);

    int i;
    for(i = 0; i < NUM_OF_WRITER; i++) {
            pthread_create(writer + i, NULL, write_thread, NULL);
    }
    for(i = 0; i < NUM_OF_WRITER; i++) {
            pthread_join(writer[i], NULL);
    }
	sleep(3);
    __sync_fetch_and_add(&end, 1);
    pthread_join(reader, NULL);

    ring_queue_destroy(&ring);
    printf("the read count (%d) the write count (%d)\n", read_count, write_count);
}

static int reads_count = 0;
static int writes_count = 0;

void *worker(void *num) {
    // printf("-------------------Worker 0x%04x starting\n", (int)pthread_self());
    struct ring_queue *rb = &g_rq;
    int pnum = *(int *)num;

	set_thread_affinity(pnum);	

    int retries = 0;
    for (;;) {
		
		if (retries >= 5000) {
            //printf("No_more_retries!!\n");
            break;
        }
        struct send_node *v = ring_queue_pop(rb);
        if (v) {
			//printf("%d v.dlen= %d\n\n", pnum, v->dlen);
            __sync_fetch_and_add(&reads_count, 1);
			ring_queue_change_node_status(v, RQUEUE_NODE_READED);
        } else {
            pthread_testcancel();
            retries++;
            usleep(250);
        }
    }
    //  printf("-------------------Worker 0x%04x leaving\n", (int)pthread_self());
    return NULL;
}

void *filler (void *num) {
    // printf("-------------------writer 0x%04x starting\n", (int)pthread_self());
    int i;
	int len;
    struct ring_queue *rb = &g_rq;
    int pnum = *(int *) num;

	set_thread_affinity(pnum+2);	

    for (i = 0; i < 5000000; i++) {
		struct send_node *p;

        while ((p = ring_queue_push(rb)) == NULL)
            usleep(250);
		len = __sync_fetch_and_add(&allcnt, 1);
		p->dlen = len;
		//printf("%d write_len= %d\n\n", pnum, len);

		ring_queue_change_node_status(p, RQUEUE_NODE_WRITED);

        __sync_fetch_and_add(&writes_count, 1);
		//printf("\twrites_count: %d %d\n", writes_count, i);
    }
    // printf("-------------------writer 0x%04x leaving\n", (int)pthread_self());
    return NULL;
}

void *infinate_worker(void *num)
{
    // printf("-------------------Worker 0x%04x starting\n", (int)pthread_self());
    struct ring_queue *rb = &g_rq;
    int pnum = *(int *)num;
    struct send_node *v = NULL;

	set_thread_affinity(pnum);	

    int retries = 0;
    for (;;) {
        v = ring_queue_pop(rb);
        if (v) {
			//printf("%d v.dlen= %d\n\n", pnum, v->dlen);
            __sync_fetch_and_add(&reads_count, 1);

			ring_queue_change_node_status(v, RQUEUE_NODE_READED);

        } else {
            pthread_testcancel();
            retries++;
            usleep(50);
        }
		
    }
    //  printf("-------------------Worker 0x%04x leaving\n", (int)pthread_self());
    return NULL;
}
void *infinate_filler (void *num) 
{
    // printf("-------------------writer 0x%04x starting\n", (int)pthread_self());
    int i;
	int len;
    struct ring_queue *rb = &g_rq;
    int pnum = *(int *) num;

	set_thread_affinity(pnum+2);	

    for (;;) {
		struct send_node *p;

        while ((p = ring_queue_push(rb)) == NULL)
            usleep(50);
		len = __sync_fetch_and_add(&allcnt, 1);
		p->dlen = len;

		ring_queue_change_node_status(p, RQUEUE_NODE_WRITED);

        __sync_fetch_and_add(&writes_count, 1);
		//printf("\twrites_count: %d %d\n", writes_count, i);
    }
    // printf("-------------------writer 0x%04x leaving\n", (int)pthread_self());
    return NULL;
}

static void start_writers(int num_writers, pthread_t *th, struct ring_queue *rb)
{
    int i;
	
    for (i = 0 ; i < num_writers; i++) {
		th_num[i] = i;
        pthread_create(&th[i], NULL, filler, (void *) &th_num[i]);
    }
}

static void start_infinite_writers(int num_writers, pthread_t *th, struct ring_queue *rb)
{
    int i;
    for (i = 0 ; i < num_writers; i++) {
        pthread_create(&th[i], NULL, infinate_filler, rb);
    }
}

static void wait_for_writers(int num_writers, pthread_t *th) 
{
    int i;
    for (i = 0 ; i < num_writers; i++) {
        pthread_join(th[i], NULL);
    }
}

static void start_infinate_readers(int num_readers, pthread_t *th, struct ring_queue *rb)
{
    int i;
    for (i = 0; i < num_readers; i++) {
		th_num[i] = i;
        pthread_create(&th[i], NULL, infinate_worker, (void *)&th_num[i]);
    }
}

static void start_readers(int num_readers, pthread_t *th, struct ring_queue *rb)
{
    int i;
    for (i = 0; i < num_readers; i++) {
		th_num[i] = i;
        pthread_create(&th[i], NULL, worker, (void *)&th_num[i]);
    }
}

static void wait_for_readers(int num_readers, pthread_t *th) 
{
    int i;
    for (i = 0 ; i < num_readers; i++) {
     //   pthread_cancel(th[i]);
        pthread_join(th[i], NULL);
    }
}

static void handler(int sig)
{
	/* Note: calling printf() from a signal handler is not
	   strictly correct, since printf() is not async-signal-safe;
	   see signal(7) */

	printf("Number of reads:%d writes:%d\n", ATOMIC_READ(reads_count), ATOMIC_READ(writes_count));
	ATOMIC_SET(reads_count, 0);	
	ATOMIC_SET(writes_count, 0);	
}


void interval_timer_print()
{
	timer_t timerid;
    struct sigevent evp;

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = handler;
    act.sa_flags = 0;

    sigemptyset(&act.sa_mask);

    if (sigaction(SIGUSR1, &act, NULL) == -1)
		errExit("fail to sigaction");

    memset(&evp, 0, sizeof(struct sigevent));
    evp.sigev_signo = SIGUSR1;
    evp.sigev_notify = SIGEV_SIGNAL;
    if (timer_create(CLOCK_MONOTONIC, &evp, &timerid) == -1)
		errExit("fail to create timer");

    struct itimerspec it;
    it.it_interval.tv_sec = 1;
    it.it_interval.tv_nsec = 0;
    it.it_value.tv_sec = 1;
    it.it_value.tv_nsec = 0;
    if (timer_settime(timerid, 0, &it, 0) == -1)
		errExit("fail to timer_settime");

	while(1);
}

int main(int argc, char **argv) {

    int rqueue_size = 40960;
	memset(&g_rq, 0, sizeof(struct ring_queue));
 
    //printf("Create a new ringbuffer (size: %d)\n", rqueue_size);
    ring_queue_init(&g_rq, rqueue_size);
	// 	exception handling
	struct ring_queue *rb = &g_rq;

    pthread_t reader_th[MAX_WORKER];
    pthread_t writer_th[MAX_FILLER];

	/////////////////////////////////////////////////////////////////////
    printf("Multi-threaded producer/consumer (%d items, parallel reads/writes)\n", rqueue_size);
    int num_writers = 2;
    start_writers(num_writers, writer_th,rb);
	sleep(1);
    int num_readers = 2;
    start_readers(num_readers, reader_th, rb);
    wait_for_writers(num_writers, writer_th);
    wait_for_readers(num_readers, reader_th);
	printf("Number_of_reads:%d___writes:%d___retries:%d\n\n", reads_count, writes_count, retries);
	/////////////////////////////////////////////////////////////////////

#if 1
	/////////////////////////////////////////////////////////////////////
    printf("Multi-threaded producer/consumer speed test:\n");
    reads_count = 0;
    writes_count = 0;
	allcnt = 0;
    num_readers = 2;
    num_writers = 2;
    start_infinate_readers(num_readers, reader_th, rb);
	start_infinite_writers(num_writers, writer_th,rb);
	interval_timer_print();	
    wait_for_writers(num_writers, writer_th);
    wait_for_readers(num_readers, reader_th);
	/////////////////////////////////////////////////////////////////////
#endif



#if 0
	/// 这些先不用
	/////////////////////////////////////////////////////////////////////
    //printf("Write fails if ringbuffer is full (RQUEUE_MODE_BLOCKING)\n");
    //do_free = 0;
    //rqueue_write(rb, "1");
    //rqueue_write(rb, "2");
    //if (rqueue_write(rb, "must_fail") == -2) 
//		printf("Write didn't fail with return-code -2\n");


	/////////////////////////////////////////////////////////////////////
    //printf("Write overwrites if ringbuffer is full (RQUEUE_MODE_OVERWRITE)\n");
    //rqueue_set_mode(rb, RQUEUE_MODE_OVERWRITE);
    //
    //int rc = rqueue_write(rb, "3");
    //if (rc == 0) 
	//	printf("Write failed with return-code %d\n", rc);
    //printf("First value is the overwritten one (XXX - this test MUST fail until the algorithm is fixed to not require size+1 pages to work)\n");
    //printf("%s\n", rqueue_read(rb));
    //rqueue_destroy(rb);


	/////////////////////////////////////////////////////////////////////
    test_multiple_writers_one_reader();
	end = 0;

#endif
	ring_queue_destroy(rb);
    exit(0);
}


