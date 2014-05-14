#include <mppaipc.h>
#include "wsr_util.h"
#include "wsr_task.h"
#include "wsr_task_functions.h"
#include "wsr_cdeque.h"

WSR_TASK_LIST_P  wsr_task_list_create(WSR_TASK_P task){

    WSR_TASK_LIST_P task_list = malloc(sizeof(WSR_TASK_LIST));

    if(task_list == NULL){
        EMSG("Memory allocation failed\n");
        return task_list;
    }

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

WSR_TASK_P wsr_task_list_search(WSR_TASK_LIST_P task_list, int task_id){

	DMSG("Searching for the task %d \n", task_id);

	while(task_list != NULL){
		if(task_list->task != NULL){
			if(task_list->task->id == task_id)
				return task_list->task;
		}
		task_list = task_list->next;
	}

	return NULL;
}


WSR_TASK_LIST_P wsr_task_list_append(WSR_TASK_LIST_P task_list1, WSR_TASK_LIST_P task_list2){

	if(task_list1 == NULL && task_list2 == NULL)
		return NULL;

	if(task_list1 == NULL && task_list2 != NULL)
		return task_list2;

	if(task_list1 != NULL && task_list2 == NULL)
		return task_list2;

	WSR_TASK_LIST_P cur_list = task_list1;
	while(cur_list->next != NULL){
		cur_list = cur_list->next;
	}

	cur_list->next = task_list2;

	return task_list1;

}

void wsr_task_list_add(WSR_TASK_LIST_P task_list, WSR_TASK_P task){

    assert(task_list != NULL);

    if(task_list != NULL && task_list->task == NULL){
            task_list->task = task;
            return;
    }

    while(task_list->next != NULL)
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

    task->num_buffers = 0;
    task->size = 0;
    task->buffer_list = NULL;
    task->dep_task_list = NULL;
    task->dep_task_ids = NULL;
    atomic_store_explicit (&task->sync_counter, 0, relaxed);

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

void wsr_task_increment_sync_counter(WSR_TASK_P task ){

    size_t sync_counter = atomic_load_explicit(&task->sync_counter, relaxed);
    atomic_store_explicit (&task->sync_counter, sync_counter+1, relaxed);

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

    wsr_task_increment_sync_counter(dep_task);

    DMSG("Sync counter of task %d = %d \n", dep_task->id, dep_task->sync_counter);

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
    //size of buffer + id + size
    task->size += buf->size + 2 * sizeof(int);

    return;
}


#ifdef COMPUTE_CLUSTER
void wsr_task_decrement_sync_counter(WSR_TASK_P task, int thread_id){

    size_t sync_counter = atomic_load_explicit(&task->sync_counter, relaxed);
    atomic_store_explicit (&task->sync_counter, sync_counter- 1, relaxed);
    if(sync_counter == 1){
    	DMSG("Sync counter is zero, adding task %d to cdeque\n", task->id);
    	cdeque_push_task(thread_id, task);
    }

    return;
}



void wsr_update_dep_tasks(WSR_TASK_P task, int thread_id){

    WSR_TASK_LIST_P task_list = task->dep_task_list;

    while(task_list != NULL){

    	DMSG("Decrement the sync counter of task %d\n", task_list->task->id);
        wsr_task_decrement_sync_counter(task_list->task, thread_id);
        task_list = task_list->next;
    }

    return;
}

void wsr_task_list_execute(WSR_TASK_LIST_P task_list){

	DMSG("Starting execution of task list\n");
	if(task_list == NULL)
		return;

//	wsr_task_execute(task_list->task);
	wsr_execute_a_task(task_list->task, 0, 1);

	wsr_task_list_execute(task_list->next);

	return;
}

#endif
