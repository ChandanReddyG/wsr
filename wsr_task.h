#ifndef __WSR_TASK_H__
#define __WSR_TASK_H__

#include "wsr_util.h"
#include "wsr_buffer.h"
#include "stdatomic.h"

//Task
typedef struct 
{

    //type of the task, used to identify the function ptr
    int type;

    //Id of the task
    int id;

    //counter to keep track of state (ready or not) 
    atomic_size_t sync_counter;

    //total number of dependent tasks
    int num_dep_tasks;

    //List of dependent tasks
    void *dep_task_list;

    int *dep_task_ids;

    //Time it took to execute
    long time;

    //Total size of required data buffers
    int size;

    //num of dep data buffers
    int num_buffers;

    //list of required data buffers
    WSR_BUFFER_LIST *buffer_list;

} WSR_TASK;

typedef  WSR_TASK* WSR_TASK_P;

//Task list
typedef struct  wsr_task_list_t{
    
    //Pointer to task
    WSR_TASK *task;

    //next dep task
    struct wsr_task_list_t *next;

} WSR_TASK_LIST;

typedef WSR_TASK_LIST* WSR_TASK_LIST_P;

WSR_TASK_LIST_P  wsr_task_list_create(WSR_TASK_P task);
void wsr_task_list_free(WSR_TASK_LIST_P task_list, int free_tasks);
void wsr_task_list_add(WSR_TASK_LIST_P task_list, WSR_TASK_P task);
void wsr_task_list_remove(WSR_TASK_LIST_P task_list, WSR_TASK_P task);
WSR_TASK_P wsr_task_alloc(int type, int task_id, int sync_counter);
void wsr_task_free(WSR_TASK_P task, int free_buffers);
void wsr_task_add_dependent_task(WSR_TASK_P task, WSR_TASK_P dep_task);
void wsr_task_add_dependent_buffer(WSR_TASK_P task, WSR_BUFFER_P buf);
void wsr_task_list_execute(WSR_TASK_LIST_P task_list);
void wsr_update_dep_tasks(WSR_TASK_P task, int thread_id);
void wsr_task_decrement_sync_counter(WSR_TASK_P task, int thread_id);
WSR_TASK_P wsr_task_list_search(WSR_TASK_LIST_P task_list, int task_id);

#endif
