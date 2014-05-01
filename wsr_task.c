#include <mppaipc.h>
#include "wsr_util.h"
#include "wsr_task.h"
#include "wsr_task_functions.h"

WSR_TASK_LIST_P  wsr_task_list_create(WSR_TASK_P task){

    WSR_TASK_LIST_P task_list = malloc(sizeof(WSR_TASK_LIST));

    if(task_list == NULL){
        EMSG("Memory allocation failed\n");
        return task_list;
    }

    if(task != NULL)
        task_list->task = task;

    task_list->next = NULL;

    return task_list;
}

void wsr_task_list_free(WSR_TASK_LIST_P task_list, int free_tasks){

    if(task_list == NULL)
        return;

    if(task_list->next != NULL)
         wsr_task_list_free(task_list->next, free_tasks);


    if(free_tasks)
        wsr_task_free(task_list->task, free_tasks);

    free(task_list);

    return;
}

void wsr_task_list_add(WSR_TASK_LIST_P task_list, WSR_TASK_P task){

    assert(task_list != NULL);

    while(task_list != NULL)
        task_list = task_list->next;

    task_list->next = wsr_task_list_create(task);

    return;
}

void wsr_task_list_remove(WSR_TASK_LIST_P task_list, WSR_TASK_P task){


    assert(task_list != NULL);

    WSR_TASK_LIST_P prev = NULL;

    int found = 0;
    while(task_list != NULL){
        if(task_list->task == task){
            found = 1;
            break;
        }

        prev = task_list;
        task_list = task_list->next;
    }

    if(!found)
        return;

    //if item to remove is not the head of list
    if(prev != NULL){
        prev->next = task_list->next;
        free(task);
    }
    else{
        //if item to remove is a head, setting it to NULL, 
        //rather than deleting it and moving the head
        task_list->task = NULL;
    }

    return;
}

WSR_TASK_P wsr_task_alloc(int type, int task_id, int sync_counter){

    WSR_TASK_P task = malloc(sizeof(WSR_TASK));
    if(task == NULL){
        EMSG("Failed to allocate memory\n");
        return NULL;
    }

    task->type = type;
    task->id = task_id;

    
    task->num_dep_tasks = 0;
    task->sync_counter = 0;
    task->num_buffers = 0;
    task->size = 0;

    return task;
}

void wsr_task_free(WSR_TASK_P task, int free_buffers){

    if(task == NULL)
        return;

    if(task->buffer_list != NULL)
        wsr_buffer_list_free(task->buffer_list, free_buffers);


    if(task->dep_task_list != NULL)
        wsr_task_list_free(task->dep_task_list, free_buffers);

    free(task);

    return;
}

void wsr_task_add_dependent_task(WSR_TASK_P task, WSR_TASK_P dep_task){

    assert(task != NULL);
    assert(dep_task != NULL);

    if(task->dep_task_list == NULL)
        task->dep_task_list = wsr_task_list_create(dep_task);
    else
        wsr_task_list_add(task->dep_task_list, dep_task);

    task->num_dep_tasks++;
    task->sync_counter++;

    return;
}
    
void wsr_task_add_dependent_buffer(WSR_TASK_P task, WSR_BUFFER_P buf){
    
    assert(task != NULL);
    assert(buf != NULL);
    

    if(task->buffer_list == NULL)
        task->buffer_list = wsr_buffer_list_create(buf);
    else
        wsr_buffer_list_add(task->buffer_list, buf);

    task->num_buffers++;
    task->size += buf->size;

    return;
}

void wsr_task_decrement_sync_counter(WSR_TASK_P task){

    task->sync_counter--;
    //TODO: Add the tasks to ready queue if count becomes zero

    return;
}

void wsr_task_execute(WSR_TASK_P task){

    WSR_TASK_FUNC foo = wsr_get_function_ptr(task->type);

   (*foo)(task->id);

    return;
}

void wsr_update_dep_tasks(WSR_TASK_P task){

    WSR_TASK_LIST_P task_list = task->dep_task_list;

    while(task_list != NULL){

        wsr_task_decrement_sync_counter(task_list->task);
        task_list = task_list->next;
    }

    return;
}

void wsr_task_list_execute(WSR_TASK_LIST_P task_list){

	if(task_list == NULL)
		return;

	wsr_task_execute(task_list->task);

	wsr_task_list_execute(task_list->next);

	return;
}
