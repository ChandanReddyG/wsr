#include <assert.h>
#include "wsr_util.h"
#include "wsr_buffer.h"
//#include <mppaipc.h>

WSR_BUFFER_P wsr_buffer_alloc(int size, int id){

    assert(size > 0);

    WSR_BUFFER_P buf = malloc(sizeof(WSR_BUFFER));
    assert(buf != NULL);

    void *buf_ptr = malloc(size);

    if(buf_ptr == NULL)
        EMSG("Memory allocation failed for the buffer %d\n", id);

    buf->size = size;
    buf->id = id;
    buf->buf = buf_ptr;

    return buf;
}

WSR_BUFFER_P wsr_buffer_create(int size, int id, void *buf_ptr){

    assert(size > 0);

    WSR_BUFFER_P buf = malloc(sizeof(WSR_BUFFER));
    assert(buf != NULL);

    buf->size = size;
    buf->id = id;
    buf->buf = buf_ptr;

    return buf;
}
void wsr_buffer_free(WSR_BUFFER_P buf){

    assert(buf != NULL);

    if(buf->buf!= NULL)
        free(buf->buf);

    free(buf);
}

WSR_BUFFER_LIST_P wsr_buffer_list_create(WSR_BUFFER_P buf){
    
    WSR_BUFFER_LIST_P ptr = malloc(sizeof(WSR_BUFFER_LIST));
    if(ptr == NULL)
        EMSG("Memory allocation falies\n");

    ptr->size = 0;
    ptr->next = NULL;
    ptr->buf_ptr = NULL;
    if(buf != NULL){
        ptr->buf_ptr = buf; 
        ptr->size = buf->size;
    }

    return ptr;
}

void wsr_buffer_list_free(WSR_BUFFER_LIST_P buffer_list, int free_buffers){

    if(buffer_list == NULL)
        return;

     if(buffer_list->next != NULL)
         wsr_buffer_list_free(buffer_list->next, free_buffers);

     if(free_buffers)
         wsr_buffer_free(buffer_list->buf_ptr);

      free(buffer_list);

      return;
}

void wsr_buffer_list_add(WSR_BUFFER_LIST_P buffer_list, WSR_BUFFER_P buf){

	DMSG("adding buffer %d to the list\n", buf->id);

    assert(buffer_list != NULL);

    buffer_list->size += buf->size;

    if(buffer_list != NULL && buffer_list->buf_ptr == NULL){
    	buffer_list->buf_ptr = buf;
    	return;
    }

    while(buffer_list->next != NULL)
       buffer_list = buffer_list->next; 

    buffer_list->next = wsr_buffer_list_create(buf);

    DMSG("Added\n");

    return;
}
 

WSR_BUFFER_P wsr_buffer_list_search(WSR_BUFFER_LIST_P buffer_list, int buf_id){

	DMSG("Searching for buffer %d \n", buf_id);

    while(buffer_list != NULL){
        if(buffer_list->buf_ptr!= NULL){

        	if(buffer_list->buf_ptr->id == buf_id)
                return buffer_list->buf_ptr;
        }

        buffer_list = buffer_list->next;
    }

    DMSG("not found\n");

    return NULL;
}


int wsr_buffer_list_num_elemnts(WSR_BUFFER_LIST_P buffer_list){

	int i = 0;
	while(buffer_list != NULL){
        if(buffer_list->buf_ptr != NULL)
            i++;

		buffer_list = buffer_list->next;
	}

	return i;

}

void wsr_buffer_list_remove(WSR_BUFFER_LIST_P buffer_list, WSR_BUFFER_P buf){

    WSR_BUFFER_LIST_P head = buffer_list;

    assert(buffer_list != NULL);

    WSR_BUFFER_LIST_P prev = NULL;
    int found = 0;
    while(buffer_list != NULL){
        if(buffer_list->buf_ptr == buf){
            found = 1;
            break;
        }
            
        prev = buffer_list;
        buffer_list = buffer_list->next; 
    }

    if(!found)
        return;

    //if item to remove is not the head
    if(prev != NULL){
        prev->next = buffer_list->next;
        head->size -= buf->size;
        free(buffer_list);
    }
    else {
        buffer_list->buf_ptr = NULL;
        buffer_list->size -= buf->size;
    }

    return;
}
        
