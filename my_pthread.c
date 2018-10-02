// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"

ucontext_t curr_context;

void init_queue(){
	*queue = malloc(no_of_queues * sizeof(struct threadControlBlock *));
	for(int i=0;i<no_of_queues;i++){
		queue[i] = malloc(sizeof(struct threadControlBlock));
		queue[i] -> tid = 0;
		queue[i] -> next = NULL;
	}
}

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	getcontext(&curr_context);
	curr_context.uc_link=0;
	curr_context.uc_stack.ss_sp=malloc(MEM);
	curr_context.uc_stack.ss_size=MEM;
	curr_context.uc_stack.ss_flags=0;
	makecontext(&curr_context, &function, 0);
	return thread;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	return 0;
};

