all: test_rq

#CFLAGS+=-D_POSIX_SOURCE -D_REENTRANT 
#CFLAGS+=-D_REENTRANT  -pthread
CFLAGS+=-g -O0
#LD_LIBRARIES += -lpthread
LDLIBS+=-lpthread -lrt

OBJS=rq.o rqueue_test.o

test_rq: $(OBJS)
	gcc $(CFLAGS) $(OBJS) $(LDFLAGS) -g -o $@ $(LDLIBS)

rqueue_test.o: rq.o

rq.o: rq.c

clean:
	rm *.o test_rq
