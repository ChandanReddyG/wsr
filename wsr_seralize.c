//#include <mppaipc.h>
#include "wsr_task.h"
#include "wsr_buffer.h"
#include "wsr_trace.h"

#define IS_DEBUG 0

int wsr_seralize_task_size(WSR_TASK_P task){

	return sizeof(WSR_TASK)  + (task->num_dep_tasks)*sizeof(int)+ (task->num_buffers) * sizeof(int);
}

int wsr_get_seralized_task_list_size(WSR_TASK_LIST_P task_list, int *num_tasks){

	if(task_list == NULL)
		return 0;

	(*num_tasks)++ ;

	return wsr_seralize_task_size(task_list->task) + wsr_get_seralized_task_list_size(task_list->next, num_tasks);

}

char *wsr_seralize_dep_task_list(WSR_TASK_P task, char*buf){

	int i =0;
	WSR_TASK_LIST_P task_list = task->dep_task_list;
	while(task_list!=NULL){
		if(task_list->task != NULL){
			memcpy(buf, &(task_list->task->id), sizeof(int));
//			DMSG("Writing dep task id %d for the task %d\n", task_list->task->id,task->id );
			buf += sizeof(int);
			i++;
		}
		task_list = task_list->next;
	}
	assert(i == task->num_dep_tasks);

	return buf;
}

char *wsr_seralize_dep_buffer_list(WSR_TASK_P task, char*buf, WSR_BUFFER_LIST_P unique_buffer_list){

	int i =0;
	WSR_BUFFER_LIST_P buffer_list = task->buffer_list;
//	DMSG("serializing task buffer list of task %d\n", task->id);

	while(buffer_list!=NULL){
		if(buffer_list->buf_ptr != NULL){

//            DMSG("Adding buffer %d to task %d\n", buffer_list->buf_ptr->id, task->id);

			memcpy(buf, &(buffer_list->buf_ptr->id), sizeof(int));
			buf += sizeof(int);

			//DMSG("searching for buffer %d to list\n", buffer_list->buf_ptr->id);
			if(!wsr_buffer_list_search(unique_buffer_list, buffer_list->buf_ptr->id)){
                //DMSG("Adding buffer %d to list\n", buffer_list->buf_ptr->id);
				wsr_buffer_list_add(unique_buffer_list, buffer_list->buf_ptr);
               // DMSG("Number of buffer in the list = %d\n", wsr_buffer_list_num_elemnts(unique_buffer_list));

            }

			i++;
		}

		buffer_list = buffer_list->next;
	}

	assert(i == task->num_buffers);
	//DMSG("done serializing task buffer list of task %d\n", task->id);

	return buf;
}

char *wsr_deseralize_dep_task_list(WSR_TASK_P task, char*buf){

//	DMSG("Num of dep task of task %d = %d\n", task->id, task->num_dep_tasks);

	task->dep_task_ids = (int *)malloc(task->num_dep_tasks * sizeof(int));
	if(task->num_dep_tasks >0 && task->dep_task_ids == NULL)
		EMSG("Memory allocation failed\n");

	if(task->num_dep_tasks > 0)
                assert(task->dep_task_ids != NULL);

	memcpy(task->dep_task_ids, buf, (task->num_dep_tasks * sizeof(int)));
	buf += (task->num_dep_tasks * sizeof(int));

	if(task->num_dep_tasks > 0){
//        DMSG("Dep task id for task %d = %d\n", task->id, task->dep_task_ids[0]);
	}

	return buf;
}

char *wsr_deseralize_dep_buffer_list(WSR_TASK_P task, char*buf){

	task->dep_buffer_ids = (int *)malloc(task->num_buffers * sizeof(int));
	if(task->num_buffers  > 0 && task->dep_buffer_ids == NULL)
		EMSG("Memory allocation failed\n");

	if(task->num_buffers > 0)
		assert(task->dep_buffer_ids != NULL);

	memcpy(task->dep_buffer_ids, buf, (task->num_buffers * sizeof(int)));
	buf += (task->num_buffers * sizeof(int));

	return buf;
}

int wsr_seralize_data_buffers(WSR_BUFFER_LIST_P data_buffer_list, char *buf){


	if(data_buffer_list == NULL)
		return 0;

	int size = data_buffer_list->buf_ptr->size;
	memcpy(buf, &size, sizeof(int));
	buf += sizeof(int);
	memcpy(buf, &data_buffer_list->buf_ptr->id, sizeof(int));
	buf += sizeof(int);
//	DMSG("buffer ptr at seralize = %lu\n", buf);
	memcpy(buf, data_buffer_list->buf_ptr->buf, size);
	buf += size;

//	double *t = (double *)buf;
//	DMSG("val [0] = %f\n", t[0]);

	if(data_buffer_list->next != NULL)
		return wsr_seralize_data_buffers(data_buffer_list->next, buf) + size + 2*sizeof(int);
	else
        return size + 2 * sizeof(int);

}

char *wsr_deseralize_data_buffers(WSR_BUFFER_LIST_P buffer_list, int num_buffers,  char*buf){

	int i = 0, size = -1, id = -1;

	for(i=0;i<num_buffers;i++){
		memcpy(&size, buf, sizeof(int));
		buf += sizeof(int);
		memcpy(&id, buf, sizeof(int));
		buf += sizeof(int);
		assert(size > 0);
		assert(id >= 0);

		wsr_buffer_list_add(buffer_list,  wsr_buffer_create(size, id, buf));
		buf += size;
	}

	return buf;
}


char *wsr_seralize_task(WSR_TASK_P task, char *buf, WSR_BUFFER_LIST_P all_buffers){

	assert(task != NULL);

	//copy the task
	memcpy(buf, task, sizeof(WSR_TASK));
	buf += sizeof(WSR_TASK);

	//copy dep task info
	buf = wsr_seralize_dep_task_list(task, buf);

	//copy required buffer ids
	buf = wsr_seralize_dep_buffer_list(task, buf, all_buffers);

	//copy the data buffer
//	wsr_seralize_data_buffers(task->buffer_list, buf);

	return buf;

}

char *wsr_deseralize_task(WSR_TASK_LIST_P task_list, char *buf){

	WSR_TASK_P task = malloc(sizeof(WSR_TASK));
	if(task == NULL)
		EMSG("Memory allocation failed\n");

	memcpy(task, buf, sizeof(WSR_TASK));
	buf += sizeof(WSR_TASK);

	buf = wsr_deseralize_dep_task_list(task, buf);

	buf = wsr_deseralize_dep_buffer_list(task, buf);

//	buf = wsr_deseralize_data_buffers(task, buf);

	wsr_task_list_add(task_list, task);

	return buf;

}

char *wsr_seralize_task_list(WSR_TASK_LIST_P task_list, char *buf, WSR_TASK_LIST_P all_buffers){

	if(task_list == NULL)
		return buf ;

	buf = wsr_seralize_task(task_list->task, buf, all_buffers);

	buf = wsr_seralize_task_list(task_list->next, buf, all_buffers);

	return buf;
}


//Serialize list of tasks and the accessed buffers
//Return the single serialized buffer and its size
//Memory for the buffer is allocated inside the function
//
int  wsr_serialize_tasks(WSR_TASK_LIST *task_list, char *buf){


	mppa_tracepoint(wsr, seralize__in);

//	DMSG("Serializing task list\n");
	int num_tasks = 0;
	int task_size = wsr_get_seralized_task_list_size(task_list, &num_tasks) + 2*sizeof(int);
	DMSG("Num of tasks = %d\n", num_tasks);

	if(buf == NULL){
		EMSG("Buffer is null\n");
		return -1;
	}

	char *begin = buf;
//	memcpy(buf, &buf_size, sizeof(int));
	buf += sizeof(int);

	memcpy(buf, &num_tasks, sizeof(int));
	buf += sizeof(int);

	WSR_BUFFER_LIST_P all_buffers = wsr_buffer_list_create(NULL);

	buf = wsr_seralize_task_list(task_list, buf, all_buffers);

	int num_buffers = wsr_buffer_list_num_elemnts(all_buffers);
	memcpy(buf,&num_buffers, sizeof(int));
    buf += sizeof(int);
//    DMSG("Number of buffers written = %d\n", num_buffers);

	int buffer_size = 0;
    if(num_buffers > 0) 
        buffer_size = wsr_seralize_data_buffers(all_buffers, buf);

	int total_size = buffer_size + task_size + sizeof(int);
	assert(total_size <= BUFFER_SIZE);
//	DMSG("buf_size  = %d\n", total_size);
	memcpy(begin, &total_size, sizeof(int));

	mppa_tracepoint(wsr, seralize__out);

	return  total_size;
}


WSR_TASK_LIST_P wsr_deseralize_tasks(char *buf, int *buf_size, int *num_tasks){

	WSR_TASK_LIST_P task_list = wsr_task_list_create(NULL);

	memcpy(buf_size, buf, sizeof(int));
	buf += sizeof(int);
//	DMSG("buf_size  = %d\n",*buf_size);

	memcpy(num_tasks, buf, sizeof(int));
	buf += sizeof(int);
//	DMSG("num of tasks received   = %d\n",*num_tasks);

	if(*num_tasks == 0)
		return NULL;

	int i = 0;
	for(i=0;i<*num_tasks;i++)
		buf = wsr_deseralize_task(task_list, buf);

	wsr_update_dep_task_list(task_list);

	int num_buffers = 0;
	memcpy(&num_buffers, buf, sizeof(int));
    buf += sizeof(int);
//    DMSG("Number of buffers receivied = %d\n", num_buffers);

	WSR_BUFFER_LIST_P buffer_list = wsr_buffer_list_create(NULL);
	wsr_deseralize_data_buffers(buffer_list, num_buffers, buf);

	wsr_update_dep_buffer_list(task_list, buffer_list);

	return task_list;
}


void wsr_update_dep_task_list(WSR_TASK_LIST_P task_list){

	WSR_TASK_LIST_P complete_list = task_list;

	while(task_list != NULL){
		if(task_list->task != NULL){
			WSR_TASK_P task = task_list->task;
			int num_dep_task = task->num_dep_tasks;
			if(num_dep_task != 0){
				assert(task->dep_task_ids != NULL);
                task->dep_task_list = wsr_task_list_create(NULL);
				for(int i = 0;i<num_dep_task;i++){
					WSR_TASK_P dep_task = wsr_task_list_search( complete_list, task->dep_task_ids[i]);
					assert(dep_task != NULL);
//					DMSG("Adding dep task %d to task %d\n", dep_task->id, task->id);
					wsr_task_list_add(task->dep_task_list, dep_task);

				}
			}

		}
		task_list = task_list->next;
	}

}


void wsr_update_dep_buffer_list(WSR_TASK_LIST_P task_list, WSR_BUFFER_LIST_P all_buffers){

	WSR_TASK_LIST_P complete_list = task_list;

	while(task_list != NULL){
		if(task_list->task != NULL){
			WSR_TASK_P task = task_list->task;
			int num_buffers = task->num_buffers;
			if(num_buffers!= 0){
				assert(task->dep_buffer_ids != NULL);
				task->buffer_list = wsr_buffer_list_create(NULL);
				for(int i = 0;i<num_buffers;i++){
					WSR_BUFFER_P buffer = wsr_buffer_list_search(all_buffers, task->dep_buffer_ids[i]);
					assert(buffer != NULL);

					wsr_buffer_list_add(task->buffer_list, buffer);

				}
			}

		}
		task_list = task_list->next;
	}

}
