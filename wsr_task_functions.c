#include "wsr_task_functions.h"
#include "wsr_task.h"
#include "wsr_cdeque.h"
#include "matmul_tasks.h"
#include "wsr_trace.h"

#include <mppaipc.h>

#define IS_DEBUG 0

int compute0(int x){

	DMSG("Compute function 0 called\n");

	return 1;
}

int compute1(int x){

	DMSG("Compute function 1 called\n");

	return 1;
}

int compute_sum(WSR_BUFFER_P a1){

	DMSG("compute sum is called\n");
	int i = 0, sum = 0;
	int  *buf = (int *)a1->buf;
	int num_elem = a1->size / sizeof(int);
	DMSG("number of elem in buffer = %d\t, val = %d\n", num_elem, buf[8]);

	for(i=0;i<num_elem;i++)
		sum += buf[i];

	DMSG("Sum = %d\n", sum);
	return sum;
}

int vector_sum(WSR_BUFFER_LIST_P buffer_list){

	DMSG("vector sum function is called\n");
	int i = 0;

	WSR_BUFFER_P a_buf = buffer_list->buf_ptr;
	assert(a_buf != NULL);
	int  *a = (int *)a_buf->buf;
	int num_elem_a = a_buf->size / sizeof(int);

	buffer_list = buffer_list->next;
	WSR_BUFFER_P b_buf = buffer_list->buf_ptr;
	assert(b_buf != NULL);
	int  *b = (int *)b_buf->buf;
	int num_elem_b = b_buf->size / sizeof(int);

	buffer_list = buffer_list->next;
	WSR_BUFFER_P c_buf = buffer_list->buf_ptr;
	assert(c_buf != NULL);
	int  *c = (int *)c_buf->buf;
	int num_elem_c = c_buf->size / sizeof(int);

	DMSG("a = %d, b = %d, c =%d\n", num_elem_a, num_elem_b, num_elem_c);
	assert((num_elem_a == num_elem_b) && (num_elem_a== num_elem_c));

	for(i=0;i<num_elem_a;i++)
		c[i] = a[i] + b[i] + 3;

	DMSG("c[i] = %d\n", c[1]);

	return 0;
}

int compute_default(int x){

	DMSG("Compute function default called\n");
//	assert(0);
//	    EMSG("Wrong task type\n");

	return 1;
}

WSR_TASK_P wsr_create_exit_task(){

	WSR_TASK_P task = wsr_task_alloc(EXIT_TASK_ID, EXIT_TASK_ID, 0);
	return task;

}

WSR_TASK_LIST_P wsr_create_exit_task_list(int num_threads){

	WSR_TASK_LIST_P task_list = wsr_task_list_create(NULL);
	for(int i =0;i<num_threads;i++)
		wsr_task_list_add(task_list, wsr_create_exit_task());

	return task_list;

}

#ifdef COMPUTE_CLUSTER

WSR_TASK_P wsr_create_executed_tasks_transfer_task(int state, int first){

	WSR_TASK_P task = wsr_task_alloc(-1, -1, 0);
	task->param = state;
	task->param1 = first;
	return task;
}

WSR_TASK_P wsr_create_deseralize_task(int state){

	WSR_TASK_P task = wsr_task_alloc(-2, -2, 0);
	task->param = state;
	return task;
}

int wsr_task_deseralize_tasks(int cur_state, int thread_id,int num_threads, int first){

	wait_till_ready_tasks_transfer_completion(cur_state, thread_id);

	int next_state = get_next_state(cur_state);

	start_async_read_of_ready_tasks(next_state, thread_id);

	int num_tasks = 0;
	WSR_TASK_LIST_P cur_tasks = deseralize_tasks(cur_state, &num_tasks);

	if(cur_tasks->task->id != EXIT_TASK_ID){

		WSR_TASK_P transfer_task = wsr_create_executed_tasks_transfer_task(cur_state, first);

		WSR_TASK_LIST_P task_list = cur_tasks;
		while(task_list != NULL){
			wsr_task_add_dependent_task(task_list->task, transfer_task);
			task_list = task_list->next;
		}
	}

		wsr_add_to_single_cdeque(cur_tasks, thread_id);
		//wsr_add_to_cdeque(cur_tasks, thread_id, num_threads, thread_id);

	return 1;
}

int wsr_async_trasnfer_executed_task(int cur_state, int thread_id, int num_threads, int first){

	int prev_state = get_prev_state(cur_state);

	if(!first)
        wait_till_executed_tasks_transfer_completion(prev_state, thread_id);

	start_async_write_of_executed_tasks(cur_state, thread_id);

	int next_state = get_next_state(cur_state);

	wsr_task_deseralize_tasks(next_state, thread_id, num_threads, 0);
//	WSR_TASK_P execute_task = wsr_create_deseralize_task(next_state);

//	wsr_add_task_to_cdeque(execute_task, thread_id);

	return 1;
}


int program_exit_task(int thread_id){

	DMSG("Program exit task called\n");
	DMSG("Exiting the thread %d \n", thread_id);

	return EXIT_TASK_ID;

}

int wsr_execute_a_task(WSR_TASK_P task, int thread_id, int num_threads){

//	printf("thread %d Started executing task = %d, sync counter = %d\n",  thread_id, task->id, task->sync_counter);

		mppa_tracepoint(wsr, task_execute__in, thread_id, task->id);
//	int cpt = __k1_read_dsu_timestamp();
	int ret = -1;

	switch (task->type)  {

	case 0:
		ret = compute0(1);
		break;
	case 1:
		ret = compute1(1);
		break;
	case 3:
		assert(task->buffer_list->buf_ptr != NULL);
		ret = compute_sum(task->buffer_list->buf_ptr);
		break;
	case 4:
		assert(task->buffer_list != NULL);
		ret = vector_sum(task->buffer_list);
		break;
	case -1:
		ret = wsr_async_trasnfer_executed_task(task->param, thread_id, num_threads, task->param1);
		break;
//	case -2:
//		ret = wsr_task_deseralize_tasks(task->param, thread_id, num_threads);
//		break;
	case MATMUL_TASK_ID:
//		printf("Executing matmul task = %d\n", task->id);
		ret = block_matrix_multiply_task(task->buffer_list, task->param);
		break;
	case EXIT_TASK_ID:
		ret = program_exit_task( thread_id);
		break;
	default:
		ret = compute_default(0);
		break;
	}

	wsr_update_dep_tasks(task, thread_id);

		mppa_tracepoint(wsr, task_execute__out, thread_id, task->id);
//	printf("thread %d Completed the execution of task = %d \n", thread_id,  task->id);

	free(task);

//	task->time = __k1_read_dsu_timestamp() - cpt;

	return ret;
}
#endif

WSR_TASK_LIST_P get_reduction_task_list(int cluster_id){

	int task_id = 3;

	WSR_TASK_P task = wsr_task_alloc(3, task_id++, 0);

	int num_elem = 10;
	int *A = malloc(num_elem * sizeof(int));
	for(int i = 0;i < num_elem; i++)
		A[i] =3;

	WSR_BUFFER_P buf = wsr_buffer_create(num_elem * sizeof(int), 0, A);
	wsr_task_add_dependent_buffer(task, buf);

	WSR_TASK_LIST_P task_list = wsr_task_list_create(task);

	return task_list;

}

WSR_TASK_LIST_P get_vector_sum_task_list(int cluster_id){

	int task_id = 0;


	WSR_TASK_LIST_P task_list = wsr_task_list_create(NULL);

	for(int i =0;i<2;i++){

		int task_id = i;

		WSR_TASK_P task = wsr_task_alloc(4, task_id++, 0);

		int num_elem = 10;

		int *A = malloc(num_elem * sizeof(int));
		for(int i = 0;i < num_elem; i++)
			A[i] =12;

		WSR_BUFFER_P buf = wsr_buffer_create(num_elem * sizeof(int), 0, A);
		wsr_task_add_dependent_buffer(task, buf);

		int *B = malloc(num_elem * sizeof(int));
		for(int i = 0;i < num_elem; i++)
			B[i] =3;

		buf = wsr_buffer_create(num_elem * sizeof(int), 0, B);
		wsr_task_add_dependent_buffer(task, buf);

		int *C = malloc(num_elem * sizeof(int));
		for(int i = 0;i < num_elem; i++)
			C[i] =100;

		buf = wsr_buffer_create(num_elem * sizeof(int), 0, C);
		wsr_task_add_dependent_buffer(task, buf);

		wsr_task_list_add(task_list, task);

//		WSR_TASK_P red_task = get_reduction_task_list(0)->task;
//
//		wsr_task_add_dependent_task(task, red_task);
//
//		wsr_task_list_add(task_list, red_task);

	}

	return task_list;
}

static int num_iter = 5;
WSR_TASK_LIST_P get_next_task_list(int cluster_id){

//	DMSG("Getting new  task list num = %d\n", num_iter);

	if(num_iter == 0)
		return NULL;

	//		num_iter--;

	//	return get_reduction_task_list(cluster_id);
	return get_vector_sum_task_list(cluster_id);

}
