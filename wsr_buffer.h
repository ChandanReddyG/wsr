#ifndef __WSR_BUFFER_H__
#define __WSR_BUFFER_H__


//data buffer
typdef struct{
    //Pointer to buffer
    void *buf;

    //size of buffer
    int size;

    //unique id 
    int id;

} WSR_BUFFER;


typedef WSR_BUFFER* WSR_BUFFER_P;


//Data buffer list
typedef struct{

    //pointer to the buffer
    WSR_BUFFER *buf_ptr;

    //Total size of all the buffers in the list
    int size;

    //next pointer
    WSR_BUFFER_LIST *next;

} WSR_BUFFER_LIST;

typedef WSR_BUFFER_LIST* WSR_BUFFER_LIST_P

#endif
