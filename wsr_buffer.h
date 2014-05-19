#ifndef __WSR_BUFFER_H__
#define __WSR_BUFFER_H__


//data buffer
typedef struct{
    //Pointer to buffer
    void *buf;

    //size of buffer
    int size;

    //unique id 
    int id;

} WSR_BUFFER;


typedef WSR_BUFFER* WSR_BUFFER_P;


//Data buffer list
struct wsr_buffer_list_t{

    //pointer to the buffer
    WSR_BUFFER *buf_ptr;

    //Total size of all the buffers in the list
    int size;

    //next pointer
    struct wsr_buffer_list_t *next;

};

typedef struct wsr_buffer_list_t WSR_BUFFER_LIST;

typedef WSR_BUFFER_LIST* WSR_BUFFER_LIST_P;


WSR_BUFFER_P wsr_buffer_alloc(int size, int id);
WSR_BUFFER_P wsr_buffer_create(int size, int id, void *buf_ptr);
void wsr_buffer_free(WSR_BUFFER_P buf);
WSR_BUFFER_LIST_P wsr_buffer_list_create(WSR_BUFFER_P buf);
void wsr_buffer_list_free(WSR_BUFFER_LIST_P buffer_list, int free_buffers);
void wsr_buffer_list_add(WSR_BUFFER_LIST_P buffer_list, WSR_BUFFER_P buf);
WSR_BUFFER_P wsr_buffer_list_search(WSR_BUFFER_LIST_P buffer_list, int buf_id);
void wsr_buffer_list_remove(WSR_BUFFER_LIST_P buffer_list, WSR_BUFFER_P buf);
int wsr_buffer_list_num_elemnts(WSR_BUFFER_LIST_P buffer_list);

#endif
