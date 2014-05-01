#include "wsr_task_functions.h"
//#include <mppaipc.h>


int compute0(int x){

    DMSG("Compute function 0 called\n");

    return 1;
}

int compute1(int x){

    DMSG("Compute function 1 called\n");

    return 1;
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


WSR_TASK_LIST_P get_next_task_list(int cluster_id){

		int task_id = 0;
		WSR_TASK_P task = wsr_task_alloc(1, task_id++, 0);
	WSR_TASK_LIST_P task_list = wsr_task_list_create(task);


       task = wsr_task_alloc(0, task_id, 0);
	wsr_task_list_add(task_list, task);

	return task_list;

}
