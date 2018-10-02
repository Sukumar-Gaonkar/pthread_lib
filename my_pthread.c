// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server:

#include "my_pthread_t.h"


#define USE_MY_PTHREAD 1	//(comment it if you want to use pthread)

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


ucontext_t curr_context;
static my_pthread_t threadNo = 0;

my_scheduler_t schd;

my_pthread_t tid_generator()
{
	return ++threadNo;
}

/* create a new thread */
int my_pthread_create(my_pthread_t *thread, pthread_attr_t *attr, void *(*function)(void *), void *arg)
{
	assert(thread != NULL);
	getcontext(&curr_context);
	thread = tid_generator();

	curr_context.uc_link = 0;
	curr_context.uc_stack.ss_sp = malloc(MEM);
	if (curr_context.uc_stack.ss_sp == NULL){
		printf("Memory Allocation Error!!!\n");
		return 1;
	}
	curr_context.uc_stack.ss_size = MEM;
	curr_context.uc_stack.ss_flags = 0;

	tcb block;
	block.tid = thread;
	block.ucontext = curr_context;
	block.next = NULL;

	if (schd.ready_queue->start != NULL)
	{
		schd.ready_queue->end->next = &block;
		schd.ready_queue->end = &block;
	}
	else
	{

		schd.ready_queue->start = &block;
		schd.ready_queue->end = &block;
	}

	makecontext(&curr_context, &function, 0);

	return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield()
{
	return 0;
};

/* terminate a thread */
void my_pthread_exit(void *value_ptr){};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr)
{
	return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr)
{
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex)
{
	assert(mutex != NULL);
	if (mutex->lock == 1)
	{
		return 0;
	}
	else
	{
		mutex->lock = 1;
		mutex->tid = 0;
	}
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex)
{
	assert(mutex != NULL);
	if (mutex->lock == 0)
	{
		return 1;
	}
	else
	{
		mutex->lock = 0;
		mutex->tid = 0;
	}
	return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex)
{
	assert(mutex != NULL);
	free(mutex);
	return 0;
};


void pf(void *arg){
	printf("%d : I Rock!!!\n",arg);
}

int main(int argc,int argv){
	int thread_num = 5;
	pthread_t *thread = (pthread_t*)malloc(thread_num*sizeof(pthread_t));
	int i;
	for (i = 0; i < thread_num; ++i)
		pthread_create(&thread[i], NULL, &pf, &i);

	tcb *tcb_holder = schd.ready_queue->start;
	while(tcb_holder != NULL){
		printf("%d,  ",tcb_holder->tid);
	}

}

