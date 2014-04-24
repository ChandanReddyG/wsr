#ifndef __WSR_TASK_H__
#define __WSR_TASK_H__

#include "wsr_util.h"
#include "wsr_buffer.h"

//Task
typedef struct 
{

    //Id of the task
    int task_id;

    //function pointer of the task to execute
    void(*task_ptr)(void);

    //sync counter to keep track of state (ready or not) 
    int sync_counter;

    //List of dependent tasks
    void *task_list;

    //Time it took to execute
    long time;

    //Total size of required data buffers
    int size;

    //list of required data buffers
    WSR_BUFFER_LIST *buffer_list;

} WSR_TASK;

typedef WSR_TASK *wsr_task_ptr;


//Task list
typedef struct {
    
    //Pointer to task
    WSR_TASK *task;

    //next dep task
    WSR_TASK_LIST *next;

} WSR_TASK_LIST;

#endif
