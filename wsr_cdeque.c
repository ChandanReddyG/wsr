#include <stddef.h>

#include "stdatomic.h"
#include "papi-defs.h"
#include "wsr_cdeque.h"
#include "wsr_task.h"
#include "wsr_trace.h"

#define MAX_NUM_OF_THREADS 16
static int num_threads = 1;
static cdeque_p cdeques[MAX_NUM_OF_THREADS];

//#define NATIVE 1

/* Push element ELEM to the bottom of the deque CDEQUE. Increase the size
   if necessary.  */
void
cdeque_push_bottom (cdeque_p cdeque, WSR_TASK_P elem, int thread_id)
{
	_PAPI_P0B;

	DMSG ("cdeque_push_bottom with elem: %d\n", elem->id);
	DMSG("thread %d Enter push top = %d, bottom = %d \n", thread_id,  __k1_umem_read32(&cdeque->top), __k1_umem_read32(&cdeque->bottom));
	mppa_tracepoint(wsr, cdeque_push__in,  __k1_umem_read32(&cdeque->top),
			__k1_umem_read32(&cdeque->bottom) );
#ifndef NATIVE
//	mppa_tracepoint(wsr, atomic_load__in);
	size_t bottom = atomic_load_explicit (&cdeque->bottom, relaxed);
//	mppa_tracepoint(wsr, atomic_load__out);

//	mppa_tracepoint(wsr, atomic_load__in);
	size_t top = atomic_load_explicit (&cdeque->top, acquire);
//	mppa_tracepoint(wsr, atomic_load__out);

	cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);


	if (bottom > top + buffer->size) {
		DMSG("resize is getting called\n");
		buffer = cbuffer_grow (buffer, bottom, top, &cdeque->cbuffer);
	}

	cbuffer_set (buffer, bottom, elem, relaxed);
	thread_fence (release);
	atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);

	mppa_tracepoint(wsr, cdeque_push__out,  __k1_umem_read32(&cdeque->top),
			__k1_umem_read32(&cdeque->bottom) );

#else
	size_t bottom = __k1_umem_read32(&cdeque->bottom);
	size_t top = __k1_umem_read32(&cdeque->top);

	cbuffer_p buffer = __k1_umem_read32(&cdeque->cbuffer);

	if (bottom >= top + __k1_umem_read32(&buffer->size)){
		DMSG("trying to grow the queue\n");
	 buffer = cbuffer_grow (buffer, bottom, top, __k1_umem_read32(&cdeque->cbuffer));
	}

	cbuffer_set (buffer, bottom, elem, relaxed);
	__k1_umem_write32(&cdeque->bottom, bottom + 1);

	DMSG("thread %d Exit push top = %d, bottom = %d \n",  thread_id, __k1_umem_read32(&cdeque->top), __k1_umem_read32(&cdeque->bottom));
	mppa_tracepoint(wsr, cdeque_push__out,  __k1_umem_read32(&cdeque->top),
			__k1_umem_read32(&cdeque->bottom) );
#endif

	_PAPI_P0E;

}

void
cdeque_push_task(int thread_id, WSR_TASK_P elem)
{
	cdeque_push_bottom(cdeques[thread_id], elem, thread_id);

}


int
cdeque_try_push_bottom (cdeque_p cdeque, WSR_TASK_P elem)
{
	_PAPI_P0B;

	assert(0);

	size_t bottom = atomic_load_explicit (&cdeque->bottom, relaxed);
	size_t top = atomic_load_explicit (&cdeque->top, acquire);

	cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);

//	DMSG ("cdeque_push_bottom with elem: %d\n", elem->id);
	if (bottom >= top + buffer->size){
//		DMSG("Buffer grow is called\n");
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
cdeque_take (cdeque_p cdeque, int thread_id)
{
	_PAPI_P1B;

	size_t bottom, top;
	WSR_TASK_P task;
	cbuffer_p buffer;

#ifndef NATIVE

	mppa_tracepoint(wsr, cdeque_take__in,  __k1_umem_read32(&cdeque->top),
			__k1_umem_read32(&cdeque->bottom) );

	if (atomic_load_explicit (&cdeque->bottom, relaxed) == 0)
	{
		/* bottom == 0 needs to be treated specially as writing
	 bottom - 1 would wrap around and allow steals to succeed
	 even though they should not. Double-loading bottom is OK
	 as we are the only thread that alters its value. */
		mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
				__k1_umem_read32(&cdeque->bottom) );

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
		mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
				__k1_umem_read32(&cdeque->bottom) );

		_PAPI_P1E;
		return NULL;
	}

	task = cbuffer_get (buffer, bottom, relaxed);

	if (bottom > top)
	{
		mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
				__k1_umem_read32(&cdeque->bottom) );

		_PAPI_P1E;
		return task;
	}

	/* One compare and swap when the deque has one single element.  */
	if (!atomic_compare_exchange_strong_explicit (&cdeque->top, &top, top + 1,
			seq_cst, relaxed))
		task = NULL;
	atomic_store_explicit (&cdeque->bottom, bottom + 1, relaxed);

	mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
			__k1_umem_read32(&cdeque->bottom) );

#else

	DMSG("thread %d queue take called\n", thread_id);

	DMSG("thread %d Enter take top = %d, bottom = %d \n", thread_id, __k1_umem_read32(&cdeque->top), __k1_umem_read32(&cdeque->bottom));

	mppa_tracepoint(wsr, cdeque_take__in,  __k1_umem_read32(&cdeque->top),
			__k1_umem_read32(&cdeque->bottom) );

	  if (__k1_umem_read32(&cdeque->bottom) == 0)
	      {
	        /* bottom == 0 needs to be treated specially as writing
	  	 bottom - 1 would wrap around and allow steals to succeed
	  	 even though they should not. Double-loading bottom is OK
	  	 as we are the only thread that alters its value. */
	        _PAPI_P1E;
	DMSG("thread %d Exit take top = %d, bottom = %d \n", thread_id,  __k1_umem_read32(&cdeque->top), __k1_umem_read32(&cdeque->bottom));
        mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
                __k1_umem_read32(&cdeque->bottom) );
	        return NULL;
	      }

	    buffer = __k1_umem_read32(&cdeque->cbuffer);
	  #if LLSC_OPTIMIZATION && defined(__arm__)
	    do
	      bottom = load_linked (&cdeque->bottom) - 1;
	    while (!store_conditional (&cdeque->bottom, bottom));
	    /* Force coherence point. */
	  #else
	    bottom = __k1_umem_read32(&cdeque->bottom) - 1;
	    __k1_umem_write32(&cdeque->bottom, bottom);
	  #endif

	    top = __k1_umem_read32(&cdeque->top);

	    if (bottom < top)
	      {
	    	__k1_umem_write32(&cdeque->bottom, bottom + 1);
	        _PAPI_P1E;
            DMSG("thread %d Exit take top = %d, bottom = %d \n", thread_id,  __k1_umem_read32(&cdeque->top), __k1_umem_read32(&cdeque->bottom));
        mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
                __k1_umem_read32(&cdeque->bottom) );
	        return NULL;
	      }

	    task = cbuffer_get (buffer, bottom, relaxed);

	    if (bottom > top)
	      {
	        _PAPI_P1E;
            DMSG("thread %d Exit take top = %d, bottom = %d \n", thread_id, __k1_umem_read32(&cdeque->top), __k1_umem_read32(&cdeque->bottom));
            mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
                __k1_umem_read32(&cdeque->bottom) );
	        return task;
	      }

	    /* One compare and swap when the deque has one single element.  */
	    if (!__k1_compare_and_swap (&cdeque->top, top, top+1))
	      task = NULL;
	    __k1_umem_write32(&cdeque->bottom, top + 1);
	DMSG("thread %d Exit take top = %d, bottom = %d \n",thread_id,  __k1_umem_read32(&cdeque->top), __k1_umem_read32(&cdeque->bottom));

	mppa_tracepoint(wsr, cdeque_take__out,  __k1_umem_read32(&cdeque->top),
			__k1_umem_read32(&cdeque->bottom) );

#endif
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

#ifndef NATIVE

//		mppa_tracepoint(wsr, cdeque_steal__in,  __k1_umem_read32(&remote_cdeque->top),
//				__k1_umem_read32(&remote_cdeque->bottom) );

	top = atomic_load_explicit (&remote_cdeque->top, acquire);
	 thread_fence (seq_cst);
	bottom = atomic_load_explicit (&remote_cdeque->bottom, acquire);

	if(top >= bottom){
		_PAPI_P2E;
		return NULL;
	}

//	DMSG ("cdeque_steal with bottom %lu, top %lu \n", bottom, top);

	buffer = atomic_load_explicit (&remote_cdeque->cbuffer, relaxed);
	elem = cbuffer_get (buffer, top, relaxed);

	if (!atomic_compare_exchange_strong_explicit (&remote_cdeque->top, &top,
			(top + 1)%buffer->size, seq_cst, relaxed))
		elem = NULL;

//		mppa_tracepoint(wsr, cdeque_steal__out,  __k1_umem_read32(&remote_cdeque->top),
//				__k1_umem_read32(&remote_cdeque->bottom) );

	_PAPI_P2E;
	return elem;

//	if(elem != NULL)
//		DMSG("Steal successfull\n");

#else
//        DMSG("top = %d, bottom = %d \n", __k1_umem_read32(&remote_cdeque->top), __k1_umem_read32(&remote_cdeque->bottom));

//	mppa_tracepoint(wsr, cdeque_steal__in,  __k1_umem_read32(&remote_cdeque->top),
//			__k1_umem_read32(&remote_cdeque->bottom) );

	  top = __k1_umem_read32(&remote_cdeque->top);

	 #if defined(__arm__)
	   /* Block until the value read from top has been propagated to all
	      other threads. */
	   store_load_fence ();
	 #else
	   //load_load_fence (top);
	 #endif
	  thread_fence (seq_cst);

	 #if LLSC_OPTIMIZATION && defined(__arm__)
	   bottom = load_linked (&remote_cdeque->bottom);
	   if (!store_conditional (&remote_cdeque->bottom, bottom))
	     return NULL;
	   /* Force coherence point. */
	 #else
	   bottom = __k1_umem_read32(&remote_cdeque->bottom);
	 #endif

	   if (top >= bottom)
	     {
	       _PAPI_P2E;
//	DMSG("top = %d, bottom = %d \n", __k1_umem_read32(&remote_cdeque->top), __k1_umem_read32(&remote_cdeque->bottom));
//	mppa_tracepoint(wsr, cdeque_steal__out,  __k1_umem_read32(&remote_cdeque->top),
//			__k1_umem_read32(&remote_cdeque->bottom) );
	       return NULL;
	     }

	   buffer = __k1_umem_read32(&remote_cdeque->cbuffer);
	   elem = cbuffer_get (buffer, top, relaxed);
	 #if defined(__arm__)
	   /* Do not reorder the previous load with the load from the CAS. */
	   load_load_fence ((uintptr_t) elem);
	 #else
	   //load_store_fence ((uintptr_t) elem);
	 #endif

	  thread_fence (seq_cst);


	   if (!__k1_compare_and_swap (&remote_cdeque->top, top, top+1))
	       elem = NULL;

//	mppa_tracepoint(wsr, cdeque_steal__out,  __k1_umem_read32(&remote_cdeque->top),
//			__k1_umem_read32(&remote_cdeque->bottom) );
//	DMSG("top = %d, bottom = %d \n", __k1_umem_read32(&remote_cdeque->top), __k1_umem_read32(&remote_cdeque->bottom));
#endif

	_PAPI_P2E;
	return elem;
}

void wsr_init_cdeques(int nb_threads ){

	num_threads = nb_threads;
	DMSG("Nb threads = %d\n", num_threads);
	assert(num_threads <= MAX_NUM_OF_THREADS);
	for(int i =0;i<num_threads;i++)
		cdeques[i] =  cdeque_alloc(7);

	return;

}

void wsr_add_task_to_cdeque(WSR_TASK_P task, int thread_id){

	size_t sync_counter = atomic_load_explicit(&task->sync_counter, relaxed);
	if(sync_counter == 0){
		DMSG("Adding task %d to task list\n", task->id);
		cdeque_push_bottom(cdeques[thread_id],task, thread_id);
	}

	return;
}

//Add task for task list to the cdeque
void wsr_add_to_cdeque(WSR_TASK_LIST_P task_list,
		int num_tasks, int nb_threads, int thread_id) {


	int i = 0;
	while(task_list != NULL){
		if(task_list->task != NULL){

			size_t sync_counter = atomic_load_explicit(&task_list->task->sync_counter, relaxed);
			if(sync_counter == 0){
				DMSG("Adding task %d to task list for threads %d, num threads = %d\n", task_list->task->id, i%num_threads, num_threads);
				cdeque_push_bottom(cdeques[i%num_threads], task_list->task, thread_id);
			}
		}

		task_list = task_list->next;
		i++;
	}
	return;
}

//Add task for task list to the cdeque
void wsr_add_to_single_cdeque(WSR_TASK_LIST_P task_list, int thread_id){

	int i = 0;
	while(task_list != NULL){
		if(task_list->task != NULL){

			size_t sync_counter = atomic_load_explicit(&task_list->task->sync_counter, relaxed);
			if(sync_counter == 0){
				DMSG("Adding task %d to task list\n", task_list->task->id);
				cdeque_push_bottom(cdeques[thread_id], task_list->task, thread_id);
			}
		}

		task_list = task_list->next;
		i++;
	}
	return;
}

void *wsr_cdeque_execute(void *arg){

	int my_thread_id = ((int *)arg)[0];

	printf("thread %d started\n", my_thread_id);

	int i = 0;
	int victim = -1;
	cdeque_p my_cdeque = cdeques[my_thread_id];

	WSR_TASK_P task = NULL;
	while(1){

		task = cdeque_take(my_cdeque, my_thread_id);

		if(task != NULL){
                DMSG("thread %d started executing task %d \n", my_thread_id, task->id);
			int ret = wsr_execute_a_task(task, my_thread_id);

			if(ret == EXIT_TASK_ID){
				DMSG("thread %d exiting the loop\n", my_thread_id);
				return 0;
			}
		}
		else {
//            sleep(0.001);

//                DMSG("thread %d take failed \n", my_thread_id);


//			DMSG("thread %d deque is empty trying to steal \n", my_thread_id);
			//my task list is empty
			i = 0;
			while(task == NULL){
//				sleep(0.1);
				//try to steal from others
				victim = my_thread_id;
				while(victim == my_thread_id){
					victim =  rand() % num_threads;
				}

//                    DMSG("Thread %d, trying to steal from %d\n", my_thread_id, victim);
//				mppa_tracepoint(wsr, try_steal__in, my_thread_id, victim);
				cdeque_p victim_dqueue = cdeques[victim];
				task = cdeque_steal(victim_dqueue);
//				mppa_tracepoint(wsr, try_steal__out, my_thread_id, victim);
//				if(task == NULL){
//                    if(i == 1000)
//                        break;

//                    i++;
//				}
			}


//			i++;

			if(task != NULL){
				DMSG("thread %d successfully  stole task %d from thread %d\n", my_thread_id, task->id, victim);
				fflush(stdout);
				int ret = wsr_execute_a_task(task, my_thread_id);

				if(ret == EXIT_TASK_ID){
					DMSG("thread %d exiting the loop\n", my_thread_id);
					return 0;
				}
			}
//			if(task != NULL)
//				cdeque_push_bottom(my_cdeque, task);
//			else
//				sleep(0.001);

            /*
				*/
//				sleep(1);
		}

	}

	return -1;

}
