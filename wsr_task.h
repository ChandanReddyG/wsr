#ifndef __WSR_TASK_H__
#define __WSR_TASK_H__

#include "wsr_util.h"
#include "wsr_buffer.h"

//Task
typedef struct 
{

    //type of the task, used to identify the function ptr
    int type;

    //Id of the task
    int id;

    //function pointer of the task to execute
    void(*task_ptr)(void);

    //counter to keep track of state (ready or not) 
    int sync_counter;

    //total number of dependent tasks
    int num_dep_tasks;

    //List of dependent tasks
    void *dep_task_list;

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
typedef struct {
    
    //Pointer to task
    WSR_TASK *task;

    //next dep task
    WSR_TASK_LIST *next;

} WSR_TASK_LIST;

typedef WSR_TASK_LIST* WSR_TASK_LIST_P;

#endif
