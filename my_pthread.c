// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"

ucontext_t curr_context, t;
uint threadNo = 0;

int pid_generator(){
	return ++threadNo;
} 

/* create a new thread */
int my_pthread_create(my_pthread_t *thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	assert(thread!=NULL);
	getcontext(&curr_context);
	thread -> ucontext = curr_context;
	thread -> tid = pid_generator();
	curr_context.uc_link=0;
	curr_context.uc_stack.ss_sp=malloc(MEM);
	curr_context.uc_stack.ss_size=MEM;
	curr_context.uc_stack.ss_flags=0;
	makecontext(&curr_context, &function, 0);
	return thread->tid;
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
	assert(mutex!=NULL);
	if(mutex->lock==1){
		return 0;
	}else{
		mutex->lock = 1;
		mutex->tid = 0;
	}
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	assert(mutex!=NULL);
	if(mutex->lock==0){
		return 1;
	}else{
		mutex->lock = 0;
		mutex->tid = 0;
	}
	return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	assert(mutex!=NULL);
	free(mutex);
	return 0;
};

