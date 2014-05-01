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


int compute_default(int x){

    DMSG("Compute function default called\n");
    EMSG("Wrong task type\n");

    return 0;
}

WSR_TASK_FUNC wsr_get_function_ptr(int task_type){

    switch (task_type)  {

    case 0:
        return &compute0;
    case 1:
        return &compute1;
    default:
        return &compute_default;
    }
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
    default:
        return compute_default(0);
    }
}

WSR_TASK_LIST_P get_next_task_list(int cluster_id){

		int task_id = 0;

		WSR_TASK_P task = wsr_task_alloc(3, task_id++, 0);

		int num_elem = 10;
		int *A = malloc(num_elem * sizeof(int));
		for(int i = 0;i < num_elem; i++)
			A[i] =3;

		WSR_BUFFER_P buf = wsr_buffer_create(num_elem * sizeof(int), 0, A);
		 wsr_task_add_dependent_buffer(task, buf);

	WSR_TASK_LIST_P task_list = wsr_task_list_create(task);


//       task = wsr_task_alloc(0, task_id, 0);
//	wsr_task_list_add(task_list, task);

	return task_list;

}
