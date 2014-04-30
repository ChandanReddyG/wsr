#include <stdio.h>
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
static const char *io_to_cc_path[PIPELINE_DEPTH];
static const char *cc_to_io_path[PIPELINE_DEPTH];

static int cc_to_io_fd[PIPELINE_DEPTH], io_to_cc_fd[PIPELINE_DEPTH];


static mppa_aiocb_t  io_to_cc_aiocb[PIPELINE_DEPTH], cc_to_io_aiocb[PIPELINE_DEPTH];

//size of buffers used in double buffering scheme: 0.5 MB
static void* buf[PIPELINE_DEPTH];

// Start the async transfer of group of tasks
void start_async_read_of_ready_tasks(int state){

        if(state<0 || state > PIPELINE_DEPTH)
			return;

        mppa_aiocb_ctor(&io_to_cc_aiocb[state], io_to_cc_fd[state], buf[state], BUFFER_SIZE);
        mppa_aio_read(&io_to_cc_aiocb[state]);

        return;
}

//Wait till the transfer is complete
void wait_till_ready_tasks_transfer_completion(int state){

        if(state<0 || state > PIPELINE_DEPTH)
			return;

		mppa_aio_wait(&io_to_cc_aiocb[state]);
        return;
}

// Start the async transfer of group of tasks
void start_async_write_of_executed_tasks(int state){

        if(state<0 || state > PIPELINE_DEPTH)
			return;

        mppa_aiocb_ctor(&cc_to_io_aiocb[state],cc_to_io_fd[state], buf[state], BUFFER_SIZE);
        mppa_aio_read(&cc_to_io_aiocb[state]);
        return;
}

//Wait till the transfer is complete
void wait_till_executed_tasks_transfer_completion(int state){

        if(state<0 || state > PIPELINE_DEPTH)
                return;

      mppa_aio_wait(&cc_to_io_aiocb[state]);
      return;

}

WSR_TASK_LIST_P deseralize_tasks(int state){

        if(state<0 || state > PIPELINE_DEPTH)
                return NULL;

        WSR_TASK_LIST_P task_list;
        task_list = wsr_deseralize_tasks(buf[state], BUFFER_SIZE);

        return task_list;
}


int main(int argc, char *argv[])
{

    cluster_id = mppa_getpid();

    DMSG("Started proc on cluster %d\n", cluster_id);

    int argn = 1;

    int i, j;
    for(i=0;i<PIPELINE_DEPTH;i++)
            io_to_cc_path[i] = argv[argn++];

    for(i=0;i<PIPELINE_DEPTH;i++)
            cc_to_io_path[i] = argv[argn++];

    const char *sync_io_to_cc = argv[argn++];
    const char *sync_cc_to_io = argv[argn++];

    for(i=0;i<PIPELINE_DEPTH;i++){
            DMSG("Open portal %s\n", io_to_cc_path[i]);
            if((io_to_cc_fd[i] = mppa_open(io_to_cc_path[i], O_RDONLY)) < 0) {
                EMSG("Open portal failed for %s\n", io_to_cc_path[i]);
                mppa_exit(1);
            }
    }


    for(i=0;i<PIPELINE_DEPTH;i++){
            DMSG("Open portal %s\n", cc_to_io_path[i]);
            if((cc_to_io_fd[i] = mppa_open(cc_to_io_path[i], O_WRONLY)) < 0) {
                EMSG("Open portal failed for %s\n", cc_to_io_path[i]);
                mppa_exit(1);
            }
    }

    for(i=0;i<PIPELINE_DEPTH;i++){
        buf[i] = memalign(64, BUFFER_SIZE);
        if (!buf[i]) {
                EMSG("Memory allocation failed: \n");
                mppa_exit(1);
        }
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

    for(i=0;i<PIPELINE_DEPTH;i++)
            free(buf[i]);

    for(i=0;i<PIPELINE_DEPTH;i++)
            mppa_close(io_to_cc_fd[i]);

    for(i=0;i<PIPELINE_DEPTH;i++)
        mppa_close(cc_to_io_fd[i]);

    mppa_exit(0);

}


