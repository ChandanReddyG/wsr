#include <stddef.h>

#include "stdatomic.h"
#include "papi-defs.h"
#include "wsr_cdeque.h"
#include "wsr_task.h"


#define MAX_NUM_OF_THREADS 16
static int num_threads = 1;
static cdeque_p cdeques[MAX_NUM_OF_THREADS];

/* Push element ELEM to the bottom of the deque CDEQUE. Increase the size
   if necessary.  */
void
cdeque_push_bottom (cdeque_p cdeque, WSR_TASK_P elem)
{
	_PAPI_P0B;


	size_t bottom = atomic_load_explicit (&cdeque->bottom, relaxed);
	size_t top = atomic_load_explicit (&cdeque->top, acquire);

	cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);

	DMSG ("cdeque_push_bottom with elem: %d\n", elem->id);
	if (bottom > top + buffer->size)
		buffer = cbuffer_grow (buffer, bottom, top, &cdeque->cbuffer);

	cbuffer_set (buffer, bottom, elem, relaxed);
	thread_fence (release);
	atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);

	_PAPI_P0E;

}

void
cdeque_push_task(int thread_id, WSR_TASK_P elem)
{
	cdeque_push_bottom(cdeques[thread_id], elem);

}


int
cdeque_try_push_bottom (cdeque_p cdeque, WSR_TASK_P elem)
{
	_PAPI_P0B;

	size_t bottom = atomic_load_explicit (&cdeque->bottom, relaxed);
	size_t top = atomic_load_explicit (&cdeque->top, acquire);

	cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);

	DMSG ("cdeque_push_bottom with elem: %d\n", elem->id);
	if (bottom >= top + buffer->size){
		//buffer = cbuffer_grow (buffer, bottom, top, &cdeque->cbuffer);
		return 0;
	}

	cbuffer_set (buffer, bottom, elem, relaxed);
	thread_fence (release);
	atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);

	_PAPI_P0E;
	return 1;
}

/* Get one task from CDEQUE for execution.  */
WSR_TASK_P
cdeque_take (cdeque_p cdeque)
{
	_PAPI_P1B;
	size_t bottom, top;
	WSR_TASK_P task;
	cbuffer_p buffer;

	if (atomic_load_explicit (&cdeque->bottom, relaxed) == 0)
	{
		/* bottom == 0 needs to be treated specially as writing
	 bottom - 1 would wrap around and allow steals to succeed
	 even though they should not. Double-loading bottom is OK
	 as we are the only thread that alters its value. */
		_PAPI_P1E;
		return NULL;
	}

	bottom = atomic_load_explicit (&cdeque->bottom, relaxed) - 1;
	buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);

	atomic_store_explicit (&cdeque->bottom, bottom, relaxed);
	thread_fence (seq_cst);

	top = atomic_load_explicit (&cdeque->top, relaxed);

	if (bottom < top)
	{
		atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);
		_PAPI_P1E;
		return NULL;
	}

	task = cbuffer_get (buffer, bottom, relaxed);

	if (bottom > top)
	{
		_PAPI_P1E;
		return task;
	}

	/* One compare and swap when the deque has one single element.  */
	if (!atomic_compare_exchange_strong_explicit (&cdeque->top, &top, top + 1,
			seq_cst, relaxed))
		task = NULL;
	atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);

	_PAPI_P1E;
	return task;
}

/* Steal one elem from deque CDEQUE. return NULL if confict happens.  */
WSR_TASK_P
cdeque_steal (cdeque_p remote_cdeque)
{
	_PAPI_P2B;
	size_t bottom, top;
	WSR_TASK_P elem;
	cbuffer_p buffer;

	top = atomic_load_explicit (&remote_cdeque->top, acquire);
	thread_fence (seq_cst);
	bottom = atomic_load_explicit (&remote_cdeque->bottom, acquire);

	DMSG ("cdeque_steal with bottom %lu, top %lu \n", bottom, top);

	if (top >= bottom)
	{
		_PAPI_P2E;
		return NULL;
	}

	buffer = atomic_load_explicit (&remote_cdeque->cbuffer, relaxed);
	elem = cbuffer_get (buffer, top, relaxed);

	if (!atomic_compare_exchange_strong_explicit (&remote_cdeque->top, &top,
			top + 1, seq_cst, relaxed))
		elem = NULL;

	_PAPI_P2E;
	return elem;
}

void wsr_init_cdeques(int nb_threads ){

	num_threads = nb_threads;
	assert(num_threads <= MAX_NUM_OF_THREADS);
	for(int i =0;i<num_threads;i++)
		cdeques[i] =  cdeque_alloc(100);

	return;

}

//Add task for task list to the cdeque
void wsr_add_to_cdeque(WSR_TASK_LIST_P task_list,
		int num_tasks, int num_threads) {


	int i = 0;
	while(task_list != NULL){
		if(task_list->task != NULL){

			size_t sync_counter = atomic_load_explicit(&task_list->task->sync_counter, relaxed);
			if(sync_counter == 0){
				DMSG("Adding task %d to task list\n", task_list->task->id);
				cdeque_push_bottom(cdeques[i%num_threads], task_list->task);
			}
		}

		task_list = task_list->next;
		i++;
	}
	return;
}


void *wsr_cdeque_execute(void *arg){

	int my_thread_id = ((int *)arg)[0];

	DMSG("thread %d started\n", my_thread_id);

	int i;
	cdeque_p my_cdeque = cdeques[my_thread_id];
	while(1){
		WSR_TASK_P task = cdeque_take(my_cdeque);

		if(task != NULL)
			wsr_execute_a_task(task, my_thread_id);

		else {
			/*
			//my task list is empty
			//try to steal from others
			for(i =0;i<2*num_threads; i++){
				int victim = my_thread_id;
				while(victim == my_thread_id){
					victim =  rand() % num_threads;
				}
				cdeque_p victim_dqueue = cdeques[victim];
				task = cdeque_steal(victim_dqueue);
				if(task != NULL){
					cdeque_push_bottom(my_cdeque, task);
					break;
				}
			}

			//termination condition
			//When the steal failed for all the other threads
			//assume execution is complete and return;
			if(i==2*num_threads)
				return 0;
			 */
			break;

		}

	}
	return 0;

}
