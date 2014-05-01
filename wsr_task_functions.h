#ifndef __WSR_TASK__FUNCTIONS_H__
#define __WSR_TASK__FUNCTIONS_H__

#include "wsr_util.h"
#include "wsr_task.h"

 typedef int (*WSR_TASK_FUNC)(int);

 WSR_TASK_FUNC wsr_get_function_ptr(int task_type);

WSR_TASK_LIST_P get_next_task_list(int cluster_id);

int wsr_execute_a_task(WSR_TASK_P task);

#endif
