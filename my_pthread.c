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
ucontext_t curr_context; /*exit_thread_context ;*/
static my_pthread_t threadNo = 1;
static int mutex_id = 0;
static int SYS_MODE = 0;
static int init = 0, timer_hit = 0;
static int NO_OF_MUTEX = 0;

static my_scheduler scheduler;
struct itimerval timeslice;
struct sigaction new_action;

int priority_level_threshold[] = { 2, 4, 6, 8, 10 };

my_pthread_mutex_t *temp_mutex;

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

	new_thread->next = NULL;
}

tcb* dequeue(tcb_list *queue) {

	tcb *curr_tcb;

	if (queue->start == NULL) {
		printf("Nothing in queue to dequeue");
		return NULL;
	}

	curr_tcb = queue->start;
	if (curr_tcb->next == NULL) {
		queue->start = NULL;
		queue->end = NULL;
	} else {
		queue->start = queue->start->next;
		if (queue->start == NULL) {
			queue->end = NULL;
		}
		curr_tcb->next = NULL;
	}

	return curr_tcb;
}

void delete_from_queue(tcb_list *queue, tcb *todel_tcb) {
	if (todel_tcb == NULL) {
		printf("Deleting NULL tcb\n");
		return;
	} else if (queue->start == NULL) {
		printf("Deleting from empty Queue\n");
	} else if (queue->start == todel_tcb) {
		queue->start = queue->start->next;
		if (queue->start == NULL)
			queue->end = NULL;
		todel_tcb->next = NULL;
		return;
	}

	tcb *curr_tcb = queue->start;
	tcb *trail_pointer = queue->start;

	while (curr_tcb != todel_tcb && curr_tcb != NULL) {
		if (curr_tcb != queue->start) {
			trail_pointer = trail_pointer->next;
		}
		curr_tcb = curr_tcb->next;
	}

	if (curr_tcb == NULL) {
		printf("Given tcb not found in Queue\n");
		return;
	} else {
		trail_pointer->next = curr_tcb->next;
		//If last tcb in the list was delete, update the end pointer
		if (curr_tcb->next == NULL)
			queue->end = trail_pointer;
		curr_tcb->next = NULL;
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

int mutex_id_generator() {
	return ++mutex_id;
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
		printf("Scheduler initiaized\n");
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
		schd_t->state = WAITING;
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

		scheduler.mutex_list = malloc(sizeof(my_pthread_mutex_t));

		if (scheduler.mutex_list == NULL) {
			printf("Mutex list init failed\n");
		}

		init_priority_queue(scheduler.priority_queue);

		enqueue(scheduler.priority_queue[0], main_t);

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

void wrapper_function(void *(*thread_function)(void *), void *arg) {
	void *ret_value = thread_function(arg);
	pthread_exit(ret_value);
}

/* create a new thread */
int my_pthread_create(my_pthread_t *thread, pthread_attr_t *attr,
		void *(*function)(void *), void *arg) {

	assert(thread != NULL);

	*thread = tid_generator();

	SYS_MODE = 1;

	make_scheduler();
	ucontext_t curr_context;
	getcontext(&curr_context);
	curr_context.uc_link = 0;
	curr_context.uc_stack.ss_sp = malloc(MEM);
	if (curr_context.uc_stack.ss_sp == NULL) {
		printf("Memory Allocation Error!!!\n");
		return 1;
	}
	curr_context.uc_stack.ss_size = MEM;
	curr_context.uc_stack.ss_flags = 0;
	makecontext(&(curr_context), &wrapper_function, 2, function, arg);

//

	// malloc ensure the tcb is created in heap and is not deallocated once the function returns.
	tcb *new_thread = (tcb *) malloc(sizeof(tcb));
	new_thread->tid = *thread;
	new_thread->ucontext = curr_context;		//test this out
	new_thread->next = NULL;
	new_thread->priority = 0;
	new_thread->recently_demoted = 0;
	new_thread->state = READY;
	new_thread->tcb_wait_queue = (tcb_list *) malloc(sizeof(tcb_list));

//	makecontext(&(new_thread->ucontext), (void *) function, 1, arg);
	enqueue(scheduler.priority_queue[0], new_thread);

	SYS_MODE = 0;

	//if timer is called midway yield the thread
	if (timer_hit == 1) {
		timer_hit = 0;
		pthread_yield();
	}

	return 0;
}

int holds_mutex(tcb *t) {
	my_pthread_mutex_t *curr_mutex = scheduler.mutex_list;
	tcb*curr_tcb;
	while (curr_mutex != NULL) {
		curr_tcb = curr_mutex->m_wait_queue->start;
		while (curr_tcb != NULL) {
			if (curr_tcb->tid == t->tid) {
				return 1;
			}
			curr_tcb = curr_tcb->next;
		}
		curr_mutex = curr_mutex->next;
	}

	return 0;
}

//void scheduling_decisions(void *arg){
//
//
//	SYS_MODE = 1;
//	make_scheduler();
//
//	// TODO: Something is wrong, we have to switch to schedulers context
//
//	tcb *prev_thread = scheduler.running_thread;
//	prev_thread->state = READY;
//
//	if (scheduler.running_thread->next != NULL)
//		scheduler.running_thread = scheduler.running_thread->next;
//	else
//		scheduler.running_thread = scheduler.priority_queue[0]->start;
//
//	while (scheduler.running_thread->state != READY) {
//		scheduler.running_thread = scheduler.running_thread->next;
//	}
//
//	ucontext_t *receiverContext = &(prev_thread->ucontext);
//	ucontext_t *nextContext = &(scheduler.running_thread->ucontext);
//	printf("rec: %p   next: %p\n", receiverContext, nextContext);
//
//	SYS_MODE = 0;
//	reset_timer();
//	assert(receiverContext != NULL);
//	assert(nextContext != NULL);
//	if (swapcontext(receiverContext, nextContext) == -1) {
//		printf("Swapcontext Failed %d %s\n", errno, strerror(errno));
//	}
//
//	return;
//}

void schd_maintenence() {
//	TODO: for loop for all levels
	static int maintenence_count = 0;
	maintenence_count++;
	int priority_level = 0;
	printf("Maintenence Cycle\n");
	// Lower the priority, more the importance.

	for (priority_level = 0; priority_level < LEVELS; priority_level++) {
		tcb *trailing_pointer = scheduler.priority_queue[priority_level]->start;
		tcb *curr_tcb = scheduler.priority_queue[priority_level]->start;
		while (curr_tcb != NULL) {
			if (!curr_tcb->recently_demoted) {
				if (curr_tcb->state == READY) {
					if (priority_level != 0)
						curr_tcb->priority = curr_tcb->priority - 1;// Aeging, priority of READY yet not in lower queue reduces.
					//				else
					//					curr_tcb->priority = curr_tcb->priority + 1;

				} else if (curr_tcb->state == WAITING&& curr_tcb->priority != MAX_PRIORITY) {
					curr_tcb->priority = curr_tcb->priority + 1;
				}
			} else {
				curr_tcb->recently_demoted = 0;
			}

			if (curr_tcb->priority > priority_level_threshold[priority_level] && priority_level < LEVELS - 1 && (priority_level <= IMP_T_DEMOTION_THRESH || (curr_tcb->tcb_wait_queue == NULL && !holds_mutex(curr_tcb)))) {
				// Demotion
				// The second part of the 'if condition' is to avoid priority inversion
				//Here we restrict important threads from falling below IMP_T_DEMOTION_THRESH priority queue level
				//Threads on which other threads are waiting and/or threads holding mutexes are important threads.

				if (curr_tcb
						== scheduler.priority_queue[priority_level]->start) {
					scheduler.priority_queue[priority_level]->start =
							curr_tcb->next;
					if (scheduler.priority_queue[priority_level]->start == NULL)
						scheduler.priority_queue[priority_level]->end = NULL;
				} else {
					trailing_pointer->next = curr_tcb->next;
				}

//				curr_tcb->next = NULL;

				tcb* to_add = curr_tcb;
				curr_tcb->recently_demoted = 1;
				curr_tcb = curr_tcb->next;

				enqueue(scheduler.priority_queue[priority_level + 1], to_add);
			} else if (curr_tcb->priority < priority_level_threshold[priority_level - 1] && priority_level != 0) {
				//Promotion
				if (curr_tcb
						== scheduler.priority_queue[priority_level]->start) {
					scheduler.priority_queue[priority_level]->start =
							curr_tcb->next;
					if (scheduler.priority_queue[priority_level]->start == NULL)
						scheduler.priority_queue[priority_level]->end = NULL;
				} else {
					trailing_pointer->next = curr_tcb->next;
				}

//				curr_tcb->next = NULL;
				tcb* to_add = curr_tcb;
				curr_tcb = curr_tcb->next;
				enqueue(scheduler.priority_queue[priority_level - 1], to_add);
			} else {
				curr_tcb = curr_tcb->next;
			}

			if (curr_tcb != scheduler.priority_queue[priority_level]->start)
				trailing_pointer = curr_tcb;

		}
	}

}

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {

	SYS_MODE = 1;
	make_scheduler();

//	if (swapcontext(&(scheduler.running_thread->ucontext), &(schd_t->ucontext)) == -1) {
//		printf("Swapcontext Failed %d %s\n", errno, strerror(errno));
//	}

	// TODO: Something is wrong, we have to switch to schedulers context

	tcb *prev_thread = scheduler.running_thread;
	if (prev_thread->state == RUNNING) {
		prev_thread->state = READY;
		prev_thread->priority = prev_thread->priority + 1;
	} else if (prev_thread->state == TERMINATED) {
		scheduler.running_thread = scheduler.running_thread->next;
		delete_from_queue(scheduler.priority_queue[0], prev_thread);
	}

	if (scheduler.running_thread != NULL
			&& scheduler.running_thread->next != NULL) {
		scheduler.running_thread = scheduler.running_thread->next;
	} else {
		// One round of priority queue completed
		schd_maintenence();

//		while (scheduler.priority_queue[0]->start == NULL){
		int flag = 1;
		while (flag) {
			tcb *temp_tcb = scheduler.priority_queue[0]->start;
			while (temp_tcb != NULL) {
				if (temp_tcb->state == READY) {
					flag = 0;
					break;
				}
				temp_tcb = temp_tcb->next;
			}
			// If nothing is there is there in the first queue
			// Call Maintenance cycle until process from lower queues eventually promote to first queue
			schd_maintenence();
		}
		scheduler.running_thread = scheduler.priority_queue[0]->start;
	}
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
	scheduler.running_thread->state = RUNNING;
	if (swapcontext(receiverContext, nextContext) == -1)
		printf("Swapcontext Failed %d %d\n", errno, strerror(errno));

	return 0;
}

void free_queue(tcb_list *list) {

	if (list->start == NULL)
		return;
	tcb *curr_tcb = list->start;
	tcb *forward_pointer = curr_tcb->next;

	while (curr_tcb != NULL) {
		if (curr_tcb->tid != 0) {
			free(curr_tcb);
			curr_tcb = forward_pointer;
			forward_pointer = forward_pointer->next;
		}
	}
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

	tcb_list *wait_queue = scheduler.running_thread->tcb_wait_queue;

	while (wait_queue->start != NULL) {
		tcb* tcb_holder = dequeue(wait_queue);
		tcb_holder->priority = 0;
		tcb_holder->state = READY;

		enqueue(scheduler.priority_queue[0], tcb_holder);
	}

	int priority_level = 0;
	if (scheduler.running_thread->tid == 0) {
		// Main thread exited, thus kill all children
		printf("Main Thread exiting, Killing all other threads");
		for (priority_level = 0;
				priority_level < LEVELS
						&& scheduler.priority_queue[priority_level]->start
								!= NULL; priority_level++) {
			free_queue(scheduler.priority_queue[priority_level]);
		}
		return;
	}

	if (scheduler.mutex_list != NULL) {
		my_pthread_mutex_t *mutex_holder = scheduler.mutex_list;
		while (mutex_holder != NULL) {
			if (mutex_holder->tid == scheduler.running_thread->tid) {
				my_pthread_mutex_unlock(mutex_holder);
			}
			mutex_holder = mutex_holder->next;
		}
	}

	scheduler.running_thread->state = TERMINATED;
//	scheduler.running_thread->tid = 2;
	scheduler.running_thread->return_val = value_ptr;

	my_pthread_yield();
}

tcb* get_tcb(my_pthread_t thread) {

	int level = 0;
	for (level = 0; level < LEVELS; level++) {
		tcb *curr_tcb = scheduler.priority_queue[level]->start;
		while (curr_tcb != NULL) {
			if (curr_tcb->tid == thread) {
				return curr_tcb;
			}
			curr_tcb = curr_tcb->next;
		}

		if (scheduler.mutex_list != NULL) {
			my_pthread_mutex_t *mutex_holder = scheduler.mutex_list;
			while (mutex_holder != NULL) {
				if (mutex_holder->m_wait_queue != NULL) {
					tcb *curr_tcb = mutex_holder->m_wait_queue->start;
					while (curr_tcb != NULL) {
						if (curr_tcb->tid == thread) {
							return curr_tcb;
						}
						curr_tcb = curr_tcb->next;
					}
				}
				mutex_holder = mutex_holder->next;
			}
		}
	}

	return NULL;
}

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {

	SYS_MODE = 1;
	make_scheduler();
	printf("Joining\n");
	if (thread == -1) {
		printf("The thread has already joined and has been terminated\n");
		return -1;
	}

	if (scheduler.running_thread->state != RUNNING) {
		printf("The thread %d is not running\n", scheduler.running_thread->tid);
		return -1;
	}

	if (scheduler.running_thread->tid == thread) {
		printf("Thread %d cannot join itself\n", thread);
		return -1;
	}

	printf("Thread %d joining thread %d\n", scheduler.running_thread->tid,
			thread);

	tcb* t;
	if ((t = get_tcb(thread)) == NULL) {
		printf("Given thread does not exist\n");
		return -1;
	}

	if (t->state == TERMINATED) {
		*value_ptr = t->return_val;
		return 0;
	} else {
		scheduler.running_thread->state = WAITING;
		delete_from_queue(scheduler.priority_queue[0],
				scheduler.running_thread);
		enqueue(t->tcb_wait_queue, scheduler.running_thread);
		my_pthread_yield();
	}

	if (value_ptr != NULL) {
		*value_ptr = t->return_val;
	}
	return 0;
}

/*Keep track of Mutexes in the system*/
int mutex_exists(my_pthread_mutex_t *mutex) {
	my_pthread_mutex_t *temp = scheduler.mutex_list;
	while (temp != NULL) {
		if (temp->id == mutex->id) {
			return 0;
		}
		temp = temp->next;
	}
	return -1;
}

void enqueue_mutex(my_pthread_mutex_t *queue, my_pthread_mutex_t *new_mutex) {

	assert(queue!=NULL);

	my_pthread_mutex_t *temp = queue;

	while (temp->next != NULL) {
		temp = temp->next;
	}

	temp->next = new_mutex;
}

void dequeue_mutex(my_pthread_mutex_t *queue) {

	assert(queue!=NULL);
	my_pthread_mutex_t *temp = queue;
	my_pthread_mutex_t *prev = NULL;

	if (temp->next != NULL) {
		prev = temp;
		temp = temp->next;
	}

	free(prev);
	queue = temp;
}

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex,
		const pthread_mutexattr_t *mutexattr) {

	SYS_MODE = 1;

	make_scheduler();

	if (mutex == NULL) {
		printf("Mutex initialization failed\n");
		return -1;
	}
	mutex->initialized = 1;
	mutex->lock = 0;
	NO_OF_MUTEX++;
	mutex->tid = 0;
	mutex->m_wait_queue = (tcb_list *) malloc(sizeof(tcb_list));
	init_queue(&(mutex->m_wait_queue));
	//printf("Scheduler mutex %p\n", scheduler.mutex_list);
	enqueue_mutex(scheduler.mutex_list, mutex);
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
			printf("Lock is already held by the current thread %d", mutex->tid);
			return -1;
		}

		if (wait_queue != NULL) {

			scheduler.running_thread->state = WAITING;
			delete_from_queue(scheduler.priority_queue[0],
					scheduler.running_thread);
			enqueue(wait_queue, scheduler.running_thread);

			pthread_yield();

			mutex->tid = scheduler.running_thread->tid;
			scheduler.running_thread->state = RUNNING;
			SYS_MODE = 0;
			return 0;
		}else{
			printf("Wait queue itself is null\n");
		}

	}

	if (mutex->lock == 0) {
		if (mutex->m_wait_queue->start == NULL) {
			mutex->lock = 1;
			mutex->tid = scheduler.running_thread->tid;
			SYS_MODE = 0;
			reset_timer();
			return 0;
		} else {
			enqueue(wait_queue, scheduler.running_thread);
			scheduler.running_thread->state = WAITING;

			pthread_yield();

			mutex->tid = scheduler.running_thread->tid;
			scheduler.running_thread->state = RUNNING;
			SYS_MODE = 0;
			return 0;
		}

	}
	SYS_MODE = 0;
	return -1;
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

		if (scheduler.running_thread->tid != mutex->tid) {
			printf("Lock owner is thread %d, cannot unlock it", mutex->tid);
			return -1;
		}

		if (scheduler.running_thread->tid == mutex->tid) {
			if (wait_queue->start == NULL) {	//Changed recently
				mutex->lock = 0;
				mutex->tid = -1;
				my_pthread_yield();
				return 0;
			} else {
				if (mutex->m_wait_queue != NULL && mutex->m_wait_queue->start) {
					tcb *new_mutex_owner = mutex->m_wait_queue->start;
					new_mutex_owner->state = READY;
					enqueue(scheduler.priority_queue[0], new_mutex_owner);
					mutex->tid = new_mutex_owner->tid;
					mutex->m_wait_queue->start =
							mutex->m_wait_queue->start->next;
					if (mutex->m_wait_queue->start == NULL)
						mutex->m_wait_queue->end = NULL;
				}
				my_pthread_yield();
				return 0;
			}
		}

	}

	return -1;
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
		mutex->initialized = 0;
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

void * dummyFunction(tcb *thread) {

	//tcb *end = scheduler.priority_queue[0]->end;
	my_pthread_t curr_threadID = thread->tid;
	printf("Entered Thread %i\n", curr_threadID);
	//scheduler.priority_queue[0]->end = end;
	int i = 0, j = 0, k = 0, l = 0;
	for (i = 0; i < 100; i++) {
		printf("Thread %d: %i\n", curr_threadID, i);

		if (i == 21 && thread->tid == 2) {
			my_pthread_mutex_lock(temp_mutex);
		} else if (i == 31 && thread->tid == 3) {
			my_pthread_mutex_lock(temp_mutex);
		}
		//scheduler.priority_queue[0]->end = end;
		for (j = 0; j < 50000; j++)
			k++;
	}
	printf("Exited Thread: %i\n", curr_threadID);
	//scheduler.priority_queue[0]->end = end;
	return &(thread->tid);
}

//void maintenance_cycle(){
//
////	tcb* temp;
//
////	 if running_time = 50 run maintenance cycle
//
////	 if run_count >= (LEVELS - priority), decrease priority, except for the last level
////	 for (i = 0; i < LEVELS-1; i++){
////		while (priority_queue[i]->tcb->start != NULL){
////			if (priority_queue[i]->tcb->start->run_count >= (LEVELS - priority)){
//
////				priority_queue[i]->tcb->start->priority -= 1;
////				priority_queue[i]->tcb->start = priority_queue[i]->tcb->start.next;
////			}
////		}
////	 }
////
//}

//int main(int argc, char **argv) {
//	pthread_t t1, t2, t3;
//	pthread_create(&t1, NULL, (void *) dummyFunction, &t1);
//	pthread_create(&t2, NULL, (void *) dummyFunction, &t2);
//	//pthread_create(&t3, NULL, (void *) dummyFunction, &t3);
//	temp_mutex = (my_pthread_mutex_t *) malloc(sizeof(my_pthread_mutex_t));
//	my_pthread_mutex_init(temp_mutex, NULL);
//
//	int xxx =  10;
//	void * ptr = &xxx;
//	void ** op_val = &ptr;
//
//	int i = 0, j = 0, k = 0, l = 0;
//	for (i = 0; i < 10; i++) {
//		printf("Main: %d\n", i);
////		if (i == 51){
////			pthread_join(t1, op_val);
////		}
//
//		for (j = 0; j < 50000; j++)
//			k++;
//	}
//
//	pthread_join(t1, op_val);
//	pthread_join(t2, op_val);
//
//	printf("Done\n");
//
//	return 0;
//}
//
