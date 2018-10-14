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
#define pthread_yield my_pthread_yield
#define pthread_mutex_init my_pthread_mutex_init
#define pthread_mutex_lock my_pthread_mutex_lock
#define pthread_mutex_unlock my_pthread_mutex_unlock
#define pthread_mutex_destroy my_pthread_mutex_destroy
#endif

tcb *schd_t, *main_t;
ucontext_t curr_context;
static my_pthread_t threadNo = 1;
static int SYS_MODE = 0;
static int init = 0, timer_hit = 0;
static int NO_OF_MUTEX = 0;

static my_scheduler scheduler;
struct itimerval timeslice;
struct sigaction new_action;

/**
 * Implementing queue functions
 **/

void init_queue(tcb_list **queue) {
	*queue = (tcb_list *) malloc(sizeof(tcb_list));
	return;
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
		free(queue->start);
	} else {
		temp = queue->start;
		queue->start = queue->start->next;
		free(temp);
	}
}

/*
 * Start of the scheduler code block
 */

void signal_handler(int signal) {
	if (SYS_MODE == 1) {
		timer_hit = 1;
		return;
	} else {
		pthread_yield();
	}
}

my_pthread_t tid_generator() {
	return ++threadNo;
}

void init_priority_queue(tcb_list *q[]) {
	int i;
	for (i = 0; i < LEVELS; i++) {
		init_queue(&(q[i]));
	}
}

void reset_timer() {
	timeslice.it_value.tv_usec = TIME_QUANTUM;
	timeslice.it_value.tv_sec = 0;
	timeslice.it_interval.tv_usec = 0;
	timeslice.it_interval.tv_sec = 0;

	if (setitimer(ITIMER_VIRTUAL, &timeslice, NULL))
		printf("Couldn't start the timer\n");
	printf("resetTimer: %d\n", timeslice.it_value.tv_usec);

}

void signalTemp() {
	pthread_yield();
}

void make_scheduler() {
	//Create context for the scheduler thread
	if (init == 0) {

		main_t = malloc(sizeof(tcb));

		if (getcontext(&(main_t->ucontext)) == -1) {
			printf("Error getting context!!!\n");
			return;
		}
		main_t->state = RUNNING;
		main_t->priority = 0;
		main_t->tid = 0;
		main_t->run_count = 0;
		main_t->tcb_wait_queue = malloc(sizeof(tcb_list));

		main_t->ucontext.uc_link = 0; //change this to maintenance cycle
		main_t->ucontext.uc_stack.ss_sp = malloc(MEM);
		if (main_t->ucontext.uc_stack.ss_sp == NULL) {
			printf("Memory Allocation Error!!!\n");
			return;
		}
		main_t->ucontext.uc_stack.ss_size = MEM;
		main_t->ucontext.uc_stack.ss_flags = 0;
		makecontext(&(main_t->ucontext), &signalTemp, 0);

		schd_t = malloc(sizeof(tcb));
		if (getcontext(&schd_t->ucontext) == -1) {
			printf("Error getting context!!!\n");
			return;
		}
		schd_t->state = WAITING; // Permanently WAITING. Ensures that the scheduler doesnt schedule itself.
		schd_t->priority = 0;
		schd_t->tid = 1;
		schd_t->run_count = 0;
		schd_t->tcb_wait_queue = malloc(sizeof(tcb_list));

		schd_t->ucontext.uc_link = 0;
		schd_t->ucontext.uc_stack.ss_sp = malloc(MEM);
		if (schd_t->ucontext.uc_stack.ss_sp == NULL) {
			printf("Memory Allocation Error!!!\n");
			return;
		}
		schd_t->ucontext.uc_stack.ss_size = MEM;
		schd_t->ucontext.uc_stack.ss_flags = 0;

		scheduler.running_thread = NULL;

		init_priority_queue(scheduler.priority_queue);

		enqueue(scheduler.priority_queue[0], main_t);
//		enqueue(scheduler.priority_queue[0], &schd_t);

		scheduler.running_thread = main_t;

		//Initialize the timer and sig alarm
		new_action.sa_handler = signal_handler;
		sigemptyset(&new_action.sa_mask);
		new_action.sa_flags = 0;
		sigaction(SIGVTALRM, &new_action, NULL);
		reset_timer();

		init = 1;

	}

}

/* create a new thread */
int my_pthread_create(my_pthread_t *thread, pthread_attr_t *attr,
		void *(*function)(void *), void *arg) {

	assert(thread != NULL);

	*thread = tid_generator();

	SYS_MODE = 1;

	make_scheduler();

	getcontext(&curr_context);
	curr_context.uc_link = &(schd_t->ucontext);
	curr_context.uc_stack.ss_sp = malloc(MEM);
	if (curr_context.uc_stack.ss_sp == NULL) {
		printf("Memory Allocation Error!!!\n");
		return 1;
	}
	curr_context.uc_stack.ss_size = MEM;
	curr_context.uc_stack.ss_flags = 0;

	// malloc ensure the tcb is created in heap and is not deallocated once the function returns.
	tcb *new_thread = (tcb *) malloc(sizeof(tcb));
	new_thread->tid = *thread;
	new_thread->ucontext = curr_context;		//test this out
	new_thread->next = NULL;
	new_thread->priority = 0;
	new_thread->state = READY;

	makecontext(&(new_thread->ucontext), (void *) function, 1, arg);
	enqueue(scheduler.priority_queue[0], new_thread);

	SYS_MODE = 0;

	//if timer is called midway yield the thread
	if (timer_hit == 1) {
		timer_hit = 0;
		pthread_yield();
	}

	return 0;
}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {

	SYS_MODE = 1;
	make_scheduler();

	// TODO: Something is wrong, we have to switch to schedulers context

	tcb *prev_thread = scheduler.running_thread;
	prev_thread->state = READY;

	if (scheduler.running_thread->next != NULL)
		scheduler.running_thread = scheduler.running_thread->next;
	else
		scheduler.running_thread = scheduler.priority_queue[0]->start;

	while (scheduler.running_thread->state != READY) {
		scheduler.running_thread = scheduler.running_thread->next;
	}

	ucontext_t *receiverContext = &(prev_thread->ucontext);
	ucontext_t *nextContext = &(scheduler.running_thread->ucontext);
	printf("rec: %p   next: %p\n", receiverContext, nextContext);

	SYS_MODE = 0;
	reset_timer();
	assert(receiverContext != NULL);
	assert(nextContext != NULL);
	if (swapcontext(receiverContext, nextContext) == -1) {
		printf("Swapcontext Failed %d %s\n", errno, strerror(errno));
	}
	return 0;
}

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
	/*
	 * Store return value in value_ptr
	 * deallocate wait queue
	 * call scheduler for the next process
	 */
	make_scheduler();
	SYS_MODE = 1;

	if (scheduler.running_thread->state == TERMINATED) {
		printf("Thread %d already terminated", scheduler.running_thread->tid);
	}

	tcb_list *temp = scheduler.running_thread->tcb_wait_queue;
	tcb* start = temp->start;

	while (start != NULL) {
		start->priority = 0;
		start->state = READY;
		start->return_val = value_ptr;
		enqueue(scheduler.priority_queue[0], start);
		start = start->next;
		dequeue(temp);
	}

	scheduler.running_thread->state = TERMINATED;
	scheduler.running_thread->tid = -1;

	my_pthread_yield();
}

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {

	SYS_MODE = 1;
	make_scheduler();

	if (thread == -1) {
		printf("The thread has already joined and has been terminated");
		return -1;
	}

	if (scheduler.running_thread->state != RUNNING) {
		printf("The thread %d is not running", scheduler.running_thread->tid);
		return -1;
	}

	if (scheduler.running_thread->tid == thread) {
		printf("Thread %d cannot join itself", thread);
		return -1;
	}

	printf("Thread %d joining thread %d", scheduler.running_thread->tid,
			thread);

	scheduler.running_thread->state = WAITING;
	enqueue(scheduler.running_thread->tcb_wait_queue, scheduler.running_thread);

	my_pthread_yield();

	if (value_ptr != NULL) {
		*value_ptr = scheduler.running_thread->return_val;
	}

	return 0;
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex,
		const pthread_mutexattr_t *mutexattr) {

	SYS_MODE = 1;

	if (mutex == NULL) {
		printf("Mutex initialization failed\n");
		return -1;
	}
	mutex->initialized = 1;
	mutex->lock = 0;
	NO_OF_MUTEX++;
	mutex->tid = 0;

	SYS_MODE = 0;
	return 0;
}

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {

	assert(mutex != NULL);

	if (mutex->initialized == 0) {
		printf("Mutex not initialized, Cannot lock it.");
		return -1;
	}

	tcb_list *wait_queue = mutex->m_wait_queue;
	SYS_MODE = 1;

	if (mutex->lock == 1) {

		if (scheduler.running_thread->tid == mutex->tid) {
			printf("Lock is already held by thread %d", mutex->tid);
			return -1;
		}

		if (wait_queue == NULL) {

			scheduler.running_thread->state = WAITING;
			enqueue(wait_queue, scheduler.running_thread);

			pthread_yield();

			mutex->tid = scheduler.running_thread->tid;
			scheduler.running_thread->state = RUNNING;
			return 0;
		}

	}

	if (mutex->lock == 0) {
		if (mutex->m_wait_queue == NULL) {
			mutex->lock = 1;
			mutex->tid = scheduler.running_thread->tid;
			reset_timer();
			return 0;
		} else {
			enqueue(wait_queue, scheduler.running_thread);
			scheduler.running_thread->state = WAITING;

			pthread_yield();

			mutex->tid = scheduler.running_thread->tid;
			scheduler.running_thread->state = RUNNING;
			return 0;
		}

	}

	return 0;
}

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	assert(mutex != NULL);

	if (mutex->initialized == 0) {
		printf("Mutex not initialized, Cannot unlock it.");
		return -1;
	}

	if (mutex->lock == 0) {
		printf("Mutex not locked, Cannot unlock it.");
		return -1;
	}

	tcb_list *wait_queue = mutex->m_wait_queue;
	SYS_MODE = 1;

	if (mutex->lock == 1) {

		if (scheduler.running_thread->tid == mutex->tid) {
			printf("Lock is already held by thread %d", mutex->tid);
			return -1;
		}

		if (wait_queue == NULL) {

			scheduler.running_thread->state = WAITING;
			enqueue(wait_queue, scheduler.running_thread);

			pthread_yield();

			mutex->tid = scheduler.running_thread->tid;
			scheduler.running_thread->state = RUNNING;
			return 0;
		}

	}

	return 0;
}

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	assert(mutex != NULL);

	SYS_MODE = 1;

	if (mutex->initialized == 0) {
		printf("Mutex is not initialized, cannot destroy");
		//call the next thread
		my_pthread_yield();
		return -1;
	}

	if (mutex->lock == 1&& mutex->tid == scheduler.running_thread->tid
	&& mutex->m_wait_queue == NULL) {
		SYS_MODE = 1;
		printf("Mutex is held by the owner %d, can destroy", mutex->tid);
		reset_timer();
		SYS_MODE = 0;
		return 0;
	}

	if (mutex->lock == 0 && mutex->tid == -1) {
		SYS_MODE = 1;
		printf("No one has the lock and no one is waiting for it, destroy...");
		mutex->initialized = 0;
		reset_timer();
		SYS_MODE = 0;
		return 0;
	}

	return 0;
}

void dummyFunction(tcb *thread) {

	//tcb *end = scheduler.priority_queue[0]->end;
	my_pthread_t curr_threadID = thread->tid;
	printf("Entered Thread %i\n", curr_threadID);
	//scheduler.priority_queue[0]->end = end;
	int i = 0, j = 0, k = 0, l = 0;
	for (i = 0; i < 100; i++) {
		printf("Thread %d: %i\n", curr_threadID, i);
		//scheduler.priority_queue[0]->end = end;
		for (j = 0; j < 50000; j++)
			k++;
	}
	printf("Exited Thread: %i\n", curr_threadID);
	//scheduler.priority_queue[0]->end = end;
	return;
}

void maintenance_cycle(){

//	tcb* temp;

//	 if running_time = 50 run maintenance cycle

//	 if run_count >= (LEVELS - priority), decrease priority, except for the last level
//	 for (i = 0; i < LEVELS-1; i++){
//		while (priority_queue[i]->tcb->start != NULL){
//			if (priority_queue[i]->tcb->start->run_count >= (LEVELS - priority)){

//				priority_queue[i]->tcb->start->priority -= 1;
//				priority_queue[i]->tcb->start = priority_queue[i]->tcb->start.next;
//			}
//		}
//	 }
//
}

int main(int argc, char **argv) {
	pthread_t t1, t2, t3;
	pthread_create(&t1, NULL, (void *) dummyFunction, &t1);
	pthread_create(&t2, NULL, (void *) dummyFunction, &t2);
	//pthread_create(&t3, NULL, (void *) dummyFunction, &t3);

	int i = 0, j = 0, k = 0, l = 0;
	for (i = 0; i < 100; i++) {
		printf("Main: %d\n", i);

		for (j = 0; j < 50000; j++)
			k++;
	}

	printf("Done\n");

	return 0;
}

