#include "wsr_util.h"
#include "wsr_buffer.h"


WSR_BUFFER_P *wsr_buffer_alloc(int size, int id){

    assert(size > 0);

    WSR_BUFFER_P buf = malloc(sizeof(WSR_BUFFER));
    assert(buf != NULL);

    void *buf_ptr = malloc(size);

    if(buf_ptr == NULL)
        EMSG("Memory allocation failed for the buffer %d\n", id);

    buf->size = size;
    buf->id = id;

    return buf;
}


void wsr_buffer_free(WSR_BUFFER_P buf){

    assert(buf != NULL);

    if(buf->buf_ptr != NULL)
        free(buf->buf_ptr);

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

void wsr_buffer_list_add(WSR_BUFFER_LIST_P buffer_list, WSR_BUFFER_P buf){

    assert(buffer_list != NULL);

    buffer_list->size += buf->size;

    while(buffer_list != NULL)
       buffer_list = buffer_list->next; 

    buffer_list->buf_ptr = buf;

    return;
}
 

WSR_BUFFER_LIST_P wsr_buffer_list_search(WSR_BUFFER_LIST_P buffer_list, WSR_BUFFER_P buf){

    while(buffer_list != NULL){
        if(buffer_list->buf == buf){
            return buffer_list; 
        }

        buffer_list = buffer_list->next;
    }

    return buffer_list;
}


void wsr_buffer_list_remove(WSR_BUFFER_LIST_P buffer_list, WSR_BUFFER_P buf){

    WSR_BUFFER_LIST_P head = buffer_list;

    assert(buffer_list != NULL);

    WSR_BUFFER_LIST_P prev == NULL;
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
        buffer_list->buf = NULL;
        buffer_list->size -= buf->size;
    }

    return;
}
        