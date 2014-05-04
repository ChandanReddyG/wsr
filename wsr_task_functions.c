#include "wsr_task_functions.h"
#include "wsr_task.h"
#include <mppaipc.h>


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
    EMSG("Wrong task type\n");

    return 0;
}


int wsr_execute_a_task(WSR_TASK_P task){

    switch (task->type)  {

    case 0:
        return compute0(1);
    case 1:
        return compute1(1);
    case 3:
    	assert(task->buffer_list->buf_ptr != NULL);
        return compute_sum(task->buffer_list->buf_ptr);
    case 4:
    	assert(task->buffer_list != NULL);
        return vector_sum(task->buffer_list);
    default:
        return compute_default(0);
    }
}

WSR_TASK_LIST_P get_reduction_task_list(int cluster_id){

		int task_id = 0;

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

	WSR_TASK_LIST_P task_list = wsr_task_list_create(task);

	return task_list;
}

static int num_iter = 5;
WSR_TASK_LIST_P get_next_task_list(int cluster_id){

    	DMSG("Getting new  task list num = %d\n", num_iter);

		if(num_iter == 0)
			return NULL;

		num_iter--;

//	return get_reduction_task_list(cluster_id);
	return get_vector_sum_task_list(cluster_id);

}
