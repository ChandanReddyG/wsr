#include <stdlib.h>
#include <malloc.h>

#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "wsr_util.h"
#include "wsr_task.h"
#include "wsr_seralize.h"

// Global Variables

//ID of compute cluster
static unsigned long cluster_id;

//Total number of clusters
static unsigned long nb_cluster;

//Number of threads to use
static unsigned long nb_threads;

//global channel IDs used for communication
static const char *channel_io_to_cc_0;
static const char *channel_io_to_cc_1;
static const char *channel_io_to_cc_2;

static const char *channel_cc_to_io_0;
static const char *channel_cc_to_io_1;
static const char *channel_cc_to_io_2;

static int channel_cc_to_io_0_fd, channel_io_to_cc_0_fd;
static int channel_cc_to_io_1_fd, channel_io_to_cc_1_fd;
static int channel_cc_to_io_2_fd, channel_io_to_cc_2_fd;

static mppa_aiocb_t  io_to_cc_0, cc_to_io_0;
static mppa_aiocb_t  io_to_cc_1, cc_to_io_1;
static mppa_aiocb_t  io_to_cc_2, cc_to_io_2;

//size of buffers used in double buffering scheme: 0.5 MB
static void* buf_0;
static void* buf_1;
static void* buf_2;

// Start the async transfer of group of tasks
void start_async_read_of_ready_tasks(int state){

	switch(state){
	case 0:
        mppa_aiocb_ctor(&io_to_cc_0, channel_io_to_cc_0_fd, buf_0, BUFFER_SIZE);
        mppa_aio_read(&io_to_cc_0);
        break;
	case 1:
        mppa_aiocb_ctor(&io_to_cc_1, channel_io_to_cc_1_fd, buf_1, BUFFER_SIZE);
        mppa_aio_read(&io_to_cc_1);
        break;
	case 2:
        mppa_aiocb_ctor(&io_to_cc_2, channel_io_to_cc_2_fd, buf_2, BUFFER_SIZE);
        mppa_aio_read(&io_to_cc_2);
        break;
	default:
        break;
	}
	return;
}

//Wait till the transfer is complete
void wait_till_ready_tasks_transfer_completion(int state){

	switch(state){
	case 0:
		mppa_aio_wait(&io_to_cc_0);
		break;
   case 1:
		mppa_aio_wait(&io_to_cc_1);
		break;
   case 2:
		mppa_aio_wait(&io_to_cc_2);
		break;
  default:
	  break;
	}
	return;
}

// Start the async transfer of group of tasks
void start_async_write_of_executed_tasks(int state){
	switch(state){
	case 0:
        mppa_aiocb_ctor(&cc_to_io_0, channel_cc_to_io_0_fd, buf_0, BUFFER_SIZE);
        mppa_aio_read(&cc_to_io_0);
        break;
	case 1:
        mppa_aiocb_ctor(&cc_to_io_1, channel_cc_to_io_1_fd, buf_1, BUFFER_SIZE);
        mppa_aio_read(&cc_to_io_1);
        break;
	case 2:
        mppa_aiocb_ctor(&cc_to_io_2, channel_cc_to_io_2_fd, buf_0, BUFFER_SIZE);
        mppa_aio_read(&cc_to_io_2);
        break;
  default:
	  break;

	}
	return;
}

//Wait till the transfer is complete
void wait_till_executed_tasks_transfer_completion(int state){
	switch(state){
	case 0:
		mppa_aio_wait(&cc_to_io_0);
		break;
   case 1:
		mppa_aio_wait(&cc_to_io_1);
		break;
   case 2:
		mppa_aio_wait(&cc_to_io_2);
		break;
  default:
	  break;
	}
	return;
}

WSR_TASK_LIST_P deseralize_tasks(int state){

	WSR_TASK_LIST_P task_list;
	switch(state){
	case 0:
		task_list = wsr_deseralize_tasks(buf_0, BUFFER_SIZE);
		break;
	case 1:
		task_list = wsr_deseralize_tasks(buf_1, BUFFER_SIZE);
		break;
	case 2:
		task_list = wsr_deseralize_tasks(buf_2, BUFFER_SIZE);
		break;
	default:
		task_list = NULL;
		break;
	}

	return task_list;
}


int main(int argc, char *argv[])
{

    cluster_id = mppa_getpid();

    DMSG("Started proc on cluster %d\n", cluster_id);

    int argn = 1;
    //const char *sync_io_to_cc = argv[argn++];
    //const char *sync_cc_to_io = argv[argn++];

    channel_io_to_cc_0 = argv[argn++];
    channel_io_to_cc_1 = argv[argn++];
    channel_io_to_cc_2 = argv[argn++];

    channel_cc_to_io_0 = argv[argn++];
    channel_cc_to_io_1 = argv[argn++];
    channel_cc_to_io_2 = argv[argn++];

    DMSG("Open portal %s\n", channel_io_to_cc_0);
    if((channel_io_to_cc_0_fd = mppa_open(channel_io_to_cc_0, O_RDONLY)) < 0) {
        EMSG("Open portal failed for %s\n", channel_io_to_cc_0);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_io_to_cc_1);
    if((channel_io_to_cc_1_fd = mppa_open(channel_io_to_cc_1, O_RDONLY)) < 0) {
        EMSG("Open portal failed for %s\n", channel_io_to_cc_1);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_io_to_cc_2);
    if((channel_io_to_cc_2_fd = mppa_open(channel_io_to_cc_2, O_RDONLY)) < 0) {
        EMSG("Open portal failed for %s\n", channel_io_to_cc_2);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_cc_to_io_0);
    if((channel_cc_to_io_0_fd = mppa_open(channel_cc_to_io_0, O_WRONLY)) < 0) {
        EMSG("Open portal failed for %s\n", channel_cc_to_io_0);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_cc_to_io_1);
    if((channel_cc_to_io_1_fd = mppa_open(channel_cc_to_io_1, O_WRONLY)) < 0) {
        EMSG("Open portal failed for %s\n", channel_cc_to_io_1);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_cc_to_io_2);
    if((channel_cc_to_io_2_fd = mppa_open(channel_cc_to_io_2, O_WRONLY)) < 0) {
        EMSG("Open portal failed for %s\n", channel_cc_to_io_2);
        mppa_exit(1);
    }

    buf_0 = memalign(64, BUFFER_SIZE);
    buf_1 = memalign(64, BUFFER_SIZE);
    buf_2 = memalign(64, BUFFER_SIZE);
	if (!buf_0|| !buf_1 || buf_2) {
		EMSG("Memory allocation failed: \n");
		mppa_exit(1);
	}

    int prev_state = -1, cur_state = 0, next_state = 1;
	start_async_read_of_ready_tasks(cur_state);

	WSR_TASK_LIST_P cur_tasks, prev_tasks;

    while(1){

    	wait_till_ready_tasks_transfer_completion(cur_state);

    	start_async_read_of_ready_tasks(next_state);

    	cur_tasks = deseralize_tasks(cur_state);

    	if(cur_tasks != NULL)
            wsr_execute_tasks(cur_tasks);

        wait_till_executed_tasks_transfer_completion(prev_state);

        if(cur_tasks == NULL)
        	break;

    	start_async_write_of_executed_tasks(cur_state);

    	prev_state = cur_state;
    	cur_state = next_state;
    	next_state =  ++next_state%3;
    }

    free(buf_0);
    free(buf_1);
    free(buf_2);

    mppa_close(channel_io_to_cc_0);
    mppa_close(channel_io_to_cc_1);
    mppa_close(channel_io_to_cc_2);

    mppa_close(channel_cc_to_io_0);
    mppa_close(channel_cc_to_io_1);
    mppa_close(channel_cc_to_io_2);

    mppa_exit(0);

}


