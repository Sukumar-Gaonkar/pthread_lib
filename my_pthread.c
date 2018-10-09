// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"

#define USE_MY_PTHREAD 1

#ifdef USE_MY_PTHREAD
#define pthread_t my_pthread_t
#define pthread_mutex_t my_pthread_mutex_t
#define pthread_create my_pthread_create
#define pthread_exit my_pthread_exit
#define pthread_join my_pthread_join
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
#endif

ucontext_t schd_context, main_context, curr_context;
static my_pthread_t threadNo = 0;
static int SYS_MODE = 0;
static int init = 1;
static int NO_OF_MUTEX = 3;

static my_scheduler scheduler;
struct itimerval timeslice;

/**
 * Implementing queue functions
 **/

void init_queue(tcb_list *queue) {

	queue = malloc(sizeof(tcb_list));

	queue->start = (tcb*) malloc(sizeof(tcb));
	if (queue->start == NULL) {
		printf("queue start initialization failed");
	} else {
		queue->start = NULL;
	}
	queue->end = (tcb*) malloc(sizeof(tcb));
	if (queue->end == NULL) {
		printf("queue end initialization failed");
	} else {
		queue->end = NULL;
	}
}

void enqueue(tcb_list *queue, tcb *new_thread) {

	if (queue->start == NULL) {
		queue->start = new_thread;
		queue->end = new_thread;
	} else {
		queue->end->next = new_thread;
		queue->end = new_thread;
	}
}

void dequeue(tcb_list *queue) {

	tcb *temp;

	if (queue->start == NULL) {
		printf("Nothing in queue to dequeue");
		return;
	}
	if (queue->start->next == NULL) {
		queue->start = NULL;
		queue->end = NULL;
	} else {
		temp = queue->start;
		queue->start = queue->start->next;
		free(temp);
	}
}

tcb* getTcb(ucontext_t t, int id) {
	tcb *temp = (tcb*) malloc(sizeof(tcb));
	temp->tid = id;
	temp->priority = 1;
	temp->state = READY;
	temp->ucontext = t;
	temp->next = NULL;
	return temp;
}

void signal_handler(int signal) {
	if (SYS_MODE == 1) {

	}
	my_pthread_yield();
}

my_pthread_t tid_generator() {
	return ++threadNo;
}

void init_priority_queue(tcb_list *q[]) {
	q = malloc(sizeof(tcb_list) * LEVELS);
	int i;
	for (i = 0; i < LEVELS; i++) {
		init_queue(q[i]);
	}
}

void make_scheduler() {
	//Create context for the scheduler thread
	getcontext(&schd_context);
	schd_context.uc_link = 0;
	schd_context.uc_stack.ss_sp = malloc(MEM);
	if (schd_context.uc_stack.ss_sp == NULL) {
		printf("Memory Allocation Error!!!\n");
		return;
	}
	schd_context.uc_stack.ss_size = MEM;
	schd_context.uc_stack.ss_flags = 0;

	scheduler.running_thread = NULL;

	//Intitialize the list of mutexes needed for the scheduler.
	my_pthread_mutex_init(scheduler.mutex, NULL);

	//Initialize the waiting and the priority queue
	init_queue(scheduler.waiting_queue);
	init_priority_queue(scheduler.priority_queue);

	//Initialize the timer and sig alarm
	timeslice.it_value.tv_usec = TIME_QUANTUM;
	timeslice.it_value.tv_sec = 0;
	timeslice.it_interval.tv_usec = 0;
	timeslice.it_interval.tv_sec = 0;
	setitimer(ITIMER_VIRTUAL, &timeslice, NULL);
	signal(SIGVTALRM, &signal_handler);
}

/* create a new thread */
int my_pthread_create(my_pthread_t *thread, pthread_attr_t *attr,
		void *(*function)(void *), void *arg) {

	if (init == 1) {
		assert(thread != NULL);
		getcontext(&main_context);
		make_scheduler();
		init = 0;
	}

	*thread = tid_generator();

	if (SYS_MODE == 1) {

		getcontext(&curr_context);
		curr_context.uc_link = 0;
		curr_context.uc_stack.ss_sp = malloc(MEM);
		if (curr_context.uc_stack.ss_sp == NULL) {
			printf("Memory Allocation Error!!!\n");
			return 1;
		}
		curr_context.uc_stack.ss_size = MEM;
		curr_context.uc_stack.ss_flags = 0;

		tcb block;
		block.tid = *thread;
		block.ucontext = curr_context;
		block.next = NULL;
		block.priority = 0;
		block.state = READY;

		if (scheduler.running_thread != NULL) {
			scheduler.waiting_queue->end->next = &block;
			scheduler.waiting_queue->end = &block;
		} else {
			scheduler.waiting_queue->start = &block;
			scheduler.waiting_queue->end = &block;
		}

		makecontext(&curr_context, &function, 0);

	}

	return 0;
}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {

	assert(scheduler.waiting_queue!=NULL);
	SYS_MODE = 1;
	getcontext(&curr_context);

	tcb_list *wait_q = (tcb_list*) scheduler.waiting_queue;

	dequeue(wait_q);
	swapcontext(&curr_context, &(wait_q->start->ucontext));

	timeslice.it_value.tv_usec = TIME_QUANTUM;
	timeslice.it_interval.tv_usec = 0;
	timeslice.it_interval.tv_sec = 0;
	setitimer(ITIMER_VIRTUAL, &timeslice, NULL);

	return 0;
}

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {

}

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	return 0;
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex,
		const pthread_mutexattr_t *mutexattr) {

	my_pthread_mutex_t *prev = NULL;
	int i;
	for (i = 0; i < NO_OF_MUTEX; i++) {
		mutex = (my_pthread_mutex_t*) malloc(sizeof(my_pthread_mutex_t));
		if (mutex == NULL) {
			printf("Mutex initialization failed");
			return 1;
		}
		mutex->initialized = 1;
		mutex->lock = 0;
		mutex->next = prev;
		mutex->tid = 0;
		prev = mutex;
	}

	return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {

	my_pthread_mutex_t *temp = mutex;
	assert(temp != NULL);

	while (temp != NULL) {
		if (temp->initialized == 1) {
			if (temp->lock == 1 && scheduler.running_thread->tid == temp->tid) {
				printf("Lock is already held by thread %d", temp->tid);
				break;
			} else {
				temp->lock = 1;
				temp->tid = mutex->tid;
			}
		} else {
			printf("Mutex not initialized");
			return -1;
		}
		temp = temp->next;
	}
	return 0;
}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	assert(mutex != NULL);
	if (mutex->lock == 0) {
		return 1;
	} else {
		mutex->lock = 0;
		mutex->tid = 0;
	}
	return 0;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	assert(mutex != NULL);
	if (mutex->lock == 1) {
		printf("Mutex is locked by thread %d", mutex->tid);
		return 1;
	}
	free(mutex);
	return 0;
}

void reset_timer() {
	timeslice.it_value.tv_sec = 0;
	timeslice.it_value.tv_usec = TIME_QUANTUM;
	timeslice.it_interval.tv_usec = 0;
	timeslice.it_interval.tv_sec = 0;
	setitimer(ITIMER_VIRTUAL, &timeslice, NULL);
}

int main(int argc, char **argv) {

	return 0;
}
