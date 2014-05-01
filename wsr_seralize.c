//#include <mppaipc.h>
#include "wsr_task.h"
#include "wsr_buffer.h"

int wsr_seralize_task_size(WSR_TASK_P task){

    return sizeof(WSR_TASK) + task->size;

}

int wsr_get_seralized_task_list_size(WSR_TASK_LIST_P task_list, int *num_tasks){

    if(task_list == NULL)
        return 0;

    (*num_tasks)++ ;

    return wsr_seralize_task_size(task_list->task) + wsr_get_seralized_task_list_size(task_list->next, num_tasks);

}

void wsr_seralize_data_buffers(WSR_BUFFER_LIST_P data_buffer_list, char *buf){


    if(data_buffer_list == NULL)
        return;

    int size = data_buffer_list->buf_ptr->size;
    memcpy(buf, &size, sizeof(int));
    buf += sizeof(int);
    memcpy(buf, &data_buffer_list->buf_ptr->id, sizeof(int));
    buf += sizeof(int);
    memcpy(buf, data_buffer_list->buf_ptr->buf, size);

    if(data_buffer_list->next != NULL)
        wsr_seralize_data_buffers(data_buffer_list->next, buf+size);

   return;

}

char *wsr_deseralize_data_buffers(WSR_TASK_P task, char*buf){

    int i = 0, size = -1, id = -1;

    int num_buffers = task->num_buffers;
    DMSG("Number of recived buffers = %d\n", num_buffers);
    task->buffer_list = wsr_buffer_list_create(NULL);

    for(i=0;i<num_buffers;i++){
        memcpy(&size, buf, sizeof(int));
    DMSG("size of recived buffers = %d\n", size);
        buf += sizeof(int);
        memcpy(&id, buf, sizeof(int));
        buf += sizeof(int);
        assert(size > 0);
        assert(id >= 0);

        int *temp = (int *)buf;

        DMSG("recv[0] = %d\n", temp[0]);
        wsr_buffer_list_add(task->buffer_list,  wsr_buffer_create(size, id, buf));
        buf += size;
    }

    return buf;
}

void wsr_seralize_task(WSR_TASK_P task, char *buf){

	assert(task != NULL);

    //copy the task 
    memcpy(buf, task, sizeof(WSR_TASK));
    buf += sizeof(WSR_TASK);
   
    //copy the data buffer
    wsr_seralize_data_buffers(task->buffer_list, buf);

}

char *wsr_deseralize_task(WSR_TASK_LIST_P task_list, char *buf){

    WSR_TASK_P task = malloc(sizeof(WSR_TASK));

    memcpy(task, buf, sizeof(WSR_TASK));
    buf += sizeof(WSR_TASK);
   
   buf = wsr_deseralize_data_buffers(task, buf); 

   wsr_task_list_add(task_list, task);

   return buf;
    
}

void wsr_seralize_task_list(WSR_TASK_LIST_P task_list, char *buf){

    if(task_list == NULL)
        return ;

    wsr_seralize_task(task_list->task, buf);

     wsr_seralize_task_list(task_list->next, buf+wsr_seralize_task_size(task_list->task));

    return;
}


//Serialize list of tasks and the accessed buffers
//Return the single serialized buffer and its size
//Memory for the buffer is allocated inside the function
//
int  wsr_serialize_tasks(WSR_TASK_LIST *task_list, char *buf){

    int num_tasks = 0;
    int buf_size = wsr_get_seralized_task_list_size(task_list, &num_tasks) + 2*sizeof(int);
    assert(buf_size < BUFFER_SIZE);
   DMSG("buf_size  = %d\n", buf_size);
   DMSG("Num of tasks = %d\n", num_tasks);

   if(buf == NULL){
	   EMSG("Buffer is null\n");
	   return -1;
   }


   memcpy(buf, &buf_size, sizeof(int));
   buf += sizeof(int);

   memcpy(buf, &num_tasks, sizeof(int));
   buf += sizeof(int);

   wsr_seralize_task_list(task_list, buf);

   return  buf_size;
}

WSR_TASK_LIST_P wsr_deseralize_tasks(char *buf, int size){

    int num_tasks = 0;
    int buf_size = 0;

    WSR_TASK_LIST_P task_list = wsr_task_list_create(NULL);

    memcpy(&buf_size, buf, sizeof(int));
    buf += sizeof(int);
   DMSG("buf_size  = %d\n", buf_size);

    memcpy(&num_tasks, buf, sizeof(int));
    buf += sizeof(int);
   DMSG("num of tasks recived   = %d\n", num_tasks);

    if(num_tasks == 0)
    	return NULL;

    int i = 0;
    for(i=0;i<num_tasks;i++)
        buf = wsr_deseralize_task(task_list, buf);

    return task_list;
}
