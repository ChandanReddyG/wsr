#ifndef __WSR_TASK__FUNCTIONS_H__
#define __WSR_TASK__FUNCTIONS_H__

#include "wsr_util.h"
#include "wsr_task.h"

 typedef int (*WSR_TASK_FUNC)(int);

 WSR_TASK_FUNC wsr_get_function_ptr(int task_type);

WSR_TASK_LIST_P get_next_task_list(int cluster_id);

int wsr_execute_a_task(WSR_TASK_P task, int thread_id, int num_threads);

void start_async_read_of_ready_tasks(int state, int thread_id);
void wait_till_ready_tasks_transfer_completion(int state, int thread_id);
void start_async_write_of_executed_tasks(int state, int thread_id);
void wait_till_executed_tasks_transfer_completion(int state, int thread_id);
WSR_TASK_LIST_P deseralize_tasks(int state, int *num_tasks);
WSR_TASK_LIST_P wsr_create_exit_task_list(int num_threads);
WSR_TASK_P wsr_create_deseralize_task(int state);
WSR_TASK_P wsr_create_executed_tasks_transfer_task(int state, int first);
int wsr_task_deseralize_tasks(int cur_state, int thread_id,int num_threads, int first);

#endif
