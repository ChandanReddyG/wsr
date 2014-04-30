#include "wsr_task_functions.h"


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