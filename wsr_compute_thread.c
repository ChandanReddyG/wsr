#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "wsr_util.h"
#include "wsr_task.h"
#include "wsr_cdeque.h"
#include "wsr_seralize.h"
#include "wsr_trace.h"


// Global Variables

//ID of compute cluster
static unsigned long cluster_id;

//Total number of clusters
static unsigned long nb_clusters;

//Number of threads to use
static unsigned long nb_threads;

static const char *nb_threads_str;
static const char *nb_clusters_str;

//global channel IDs used for communication
static const char *io_to_cc_path[PIPELINE_DEPTH];
static const char *cc_to_io_path[PIPELINE_DEPTH];

static int cc_to_io_fd[PIPELINE_DEPTH], io_to_cc_fd[PIPELINE_DEPTH];


static mppa_aiocb_t  io_to_cc_aiocb[PIPELINE_DEPTH], cc_to_io_aiocb[PIPELINE_DEPTH];

//size of buffers used in double buffering scheme: 0.5 MB
static void* buf[PIPELINE_DEPTH];
static int buf_size[PIPELINE_DEPTH];

/////////////////////////////////////////////
// Synchronization IO / Clusters functions //
/////////////////////////////////////////////

// These functions are used to synchronize all clusters after spawn
// They may be used for further global synchronization

// Initialization of the barrier
static int
mppa_init_barrier(const char *sync_io_to_clusters_path,
		const char *sync_clusters_to_io_path,
		int *sync_io_to_cluster_fd,
		int *sync_clusters_to_io_fd)
{

	*sync_io_to_cluster_fd = mppa_open(sync_io_to_clusters_path, O_RDONLY);
	if (*sync_io_to_cluster_fd < 0) {
		EMSG("Opening %s failed!\n", sync_io_to_clusters_path);
		return 1;
	}

	unsigned long long match = -(1LL << 1);
	if (mppa_ioctl(*sync_io_to_cluster_fd, MPPA_RX_SET_MATCH, match) < 0) {
		EMSG("ioctl MPPA_RX_SET_MATCH failed on %s\n", sync_io_to_clusters_path);
		return 1;
	}

	*sync_clusters_to_io_fd = mppa_open(sync_clusters_to_io_path, O_WRONLY);
	if (*sync_clusters_to_io_fd < 0) {
		EMSG("Opening %s failed!\n", sync_clusters_to_io_path);
		return 1;
	}

	return 0;
}

// Barrier between IO Cluster and all Clusters
static int
mppa_barrier(int sync_io_to_cluster_fd, int sync_clusters_to_io_fd)
{
	//DMSG("[Cluster %d] mppa_barrier...", mppa_getpid());
	int rank = mppa_getpid();
	long long mask = 1ULL << rank;
	if (mppa_write(sync_clusters_to_io_fd, &mask, sizeof(mask)) != sizeof(mask)) {
		EMSG("mppa_write barrier failed!\n");
		return 1;
	}

	unsigned long long dummy;
	if (mppa_read(sync_io_to_cluster_fd, &dummy, sizeof(dummy)) != sizeof(unsigned long long)) {
		EMSG("mppa_read barrier failed!\n");
		return 1;
	}

	//DMSG("done\n");
	return 0;
}

// Close barrier connectors
static void
mppa_close_barrier(int sync_io_to_cluster_fd, int sync_clusters_to_io_fd)
{
	mppa_close(sync_clusters_to_io_fd);
	mppa_close(sync_io_to_cluster_fd);
}

// Start the async transfer of group of tasks
void start_async_read_of_ready_tasks(int state, int thread_id){

	if(state<0 || state > PIPELINE_DEPTH)
		return;

	mppa_tracepoint(wsr, start_sync_read_of_read_tasks, thread_id, state);

	mppa_aiocb_ctor(&io_to_cc_aiocb[state], io_to_cc_fd[state], buf[state], buf_size[state]);
	mppa_aiocb_set_trigger(&io_to_cc_aiocb[state], 1);
	int status = mppa_aio_read(&io_to_cc_aiocb[state]);
	assert(status == 0);
	DMSG("Starting the async read of ready task for cur_state = %d ret = %d\n", state, status);

	return;
}

//Wait till the transfer is complete
void wait_till_ready_tasks_transfer_completion(int state, int thread_id){

	if(state<0 || state > PIPELINE_DEPTH)
		return;

	mppa_tracepoint(wsr, wait_till_ready_task_transfer_completion__in, thread_id, state);

	DMSG("waiting for the ready task transfer to complete for state = %d\n", state);
	int status = mppa_aio_wait(&io_to_cc_aiocb[state]);
	assert(status == buf_size[state]);
	mppa_tracepoint(wsr, wait_till_ready_task_transfer_completion__out, thread_id, state);
	DMSG(" the ready task transfer is complete from io to cc for state = %d, ret = %d\n", state, status);
	return;
}

// Start the async transfer of group of tasks
void start_async_write_of_executed_tasks(int state, int thread_id){


	if(state<0 || state > PIPELINE_DEPTH)
		return;

	mppa_tracepoint(wsr, start_async_write_of_executed_tasks, thread_id, state);

	assert(buf_size[state] >= 0);
	DMSG("Sending buf size = %d\n", buf_size[state]);
	mppa_aiocb_ctor(&cc_to_io_aiocb[state], cc_to_io_fd[state], buf[state], buf_size[state]);
	mppa_aiocb_set_pwrite(&cc_to_io_aiocb[state], buf[state], buf_size[state], 0);
	int status = mppa_aio_write(&cc_to_io_aiocb[state]);
	assert(status == 0);
	DMSG("Starting the async write of executed task for cur_state = %d, ret = %d\n", state, status);

	return;
}

//Wait till the transfer is complete
void wait_till_executed_tasks_transfer_completion(int state, int thread_id){

	DMSG("waiting for the executed task transfer to complete for state = %d\n", state);
	if(state<0 || state > PIPELINE_DEPTH)
		return;

	mppa_tracepoint(wsr, wait_till_executed_task_transfer_completion__in, thread_id, state);

	int status = mppa_aio_wait(&cc_to_io_aiocb[state]);
	assert(status == buf_size[state]);
	mppa_tracepoint(wsr, wait_till_executed_task_transfer_completion__out, thread_id, state);
	DMSG(" the executed task transfer is complete from cc to io for state = %d, ret = %d\n", state, status);
	return;
}

WSR_TASK_LIST_P deseralize_tasks(int state, int *num_tasks){

	DMSG("deserialing the tasks list\n");

	if(state<0 || state > PIPELINE_DEPTH)
		return NULL;

	mppa_tracepoint(wsr, deseralize__in);

//	memcpy(&buf_size[state], buf[state], sizeof(int));

	WSR_TASK_LIST_P task_list;
	task_list = wsr_deseralize_tasks(buf[state], &buf_size[state], num_tasks);

	mppa_tracepoint(wsr, deseralize__out);

	return task_list;
}

void execute_tasks(WSR_TASK_LIST_P task_list, int num_tasks, int num_threads){

	wsr_add_to_cdeque(task_list, num_tasks, num_threads, 0);

	//start compute cluster threads
	int res = -1, i;
	pthread_t t[num_threads];
	int param[num_threads];
	for(i=1;i<num_threads; i++){
		param[i] = i;
		while(1) {
			res = pthread_create(&t[i], NULL, wsr_cdeque_execute, &param[i]);
			if( res == 0) {
				break;
			}
			if( res == -EAGAIN ){
				usleep(100000);
			} else {
				EMSG("pthread_create failed i %d, res = %d\n", i, res);
				exit(-1);
			}
		}
		DMSG("pthread create launched thread %d, locally called %d\n", (int) t[i], i);
	}

	int parm = 0;
	wsr_cdeque_execute(&parm);

	int ret_code = -1;
	void *ret;
	for (i = 1; i < num_threads; i++) {
		pthread_join(t[i], &ret);

		ret_code = ((int *)ret)[0];
//		if(ret_code != 0){
//			EMSG("pthread return code for %d failed\n", i);
//			exit(-1);
//		}
	}
}

int main(int argc, char *argv[])
{
	cluster_id = mppa_getpid();

	//DMSG("Started proc on cluster %lu\n", cluster_id);

	mppa_tracepoint(wsr, cc_main__in, cluster_id);

	int argn = 1;

	int i;
	for(i=0;i<PIPELINE_DEPTH;i++)
		io_to_cc_path[i] = argv[argn++];

	for(i=0;i<PIPELINE_DEPTH;i++)
		cc_to_io_path[i] = argv[argn++];

	const char *sync_io_to_cc_path = argv[argn++];
	const char *sync_cc_to_io_path = argv[argn++];

	nb_clusters_str = argv[argn++];
	nb_threads_str = argv[argn++];

	nb_clusters = convert_str_to_ul(nb_clusters_str);
	nb_threads = convert_str_to_ul(nb_threads_str);

	for(i=0;i<PIPELINE_DEPTH;i++){
		buf[i] = memalign(64, BUFFER_SIZE);
		if (!buf[i]) {
			EMSG("Memory allocation failed: \n");
			mppa_exit(1);
		}
		buf_size[i] = 0;
	}

	wsr_init_cdeques(nb_threads);

	mppa_tracepoint(wsr,cc_portal_open__in, cluster_id);

	//DMSG("Opening the barrier\n");
	int sync_io_to_cc_fd = -1;
	int sync_cc_to_io_fd = -1;
	if (mppa_init_barrier(sync_io_to_cc_path, sync_cc_to_io_path, &sync_io_to_cc_fd,
			&sync_cc_to_io_fd)) {
		EMSG("mppa_init_barrier failed\n");
		mppa_exit(1);
	}

	//DMSG("Opening the io to cc portals\n");
	for(i=0;i<PIPELINE_DEPTH;i++){
		//DMSG("Open portal %s\n", io_to_cc_path[i]);
		if((io_to_cc_fd[i] = mppa_open(io_to_cc_path[i], O_RDONLY)) < 0) {
			EMSG("Open portal failed for %s\n", io_to_cc_path[i]);
			mppa_exit(1);
		}
	}

	//DMSG("Opening the cc to io portals\n");
	for(i=0;i<PIPELINE_DEPTH;i++){
		//DMSG("Open portal %s\n", cc_to_io_path[i]);
		if((cc_to_io_fd[i] = mppa_open(cc_to_io_path[i], O_WRONLY)) < 0) {
			EMSG("Open portal failed for %s\n", cc_to_io_path[i]);
			mppa_exit(1);
		}
	}

	mppa_tracepoint(wsr,cc_portal_open__out, cluster_id);

	mppa_tracepoint(wsr,cc_sync__in, cluster_id);
	//DMSG("sync with io cluster complete\n");
	if (mppa_barrier(sync_io_to_cc_fd, sync_cc_to_io_fd)) {
		EMSG("mppa_barrier failed\n");
		mppa_exit(1);
	}
	mppa_tracepoint(wsr,cc_sync__out, cluster_id);


	buf_size[0] = BUFFER_SIZE;
	buf_size[1] = BUFFER_SIZE;
	buf_size[2] = BUFFER_SIZE;

	wsr_init_cdeques(nb_threads);

	WSR_TASK_LIST_P cur_tasks;

//	for(i=0;i<3;i++){
//	start_async_read_of_ready_tasks(0);
//
//	wait_till_ready_tasks_transfer_completion(0);
//
//	cur_tasks = deseralize_tasks(0);
//
//	if(cur_tasks != NULL)
//		wsr_task_list_execute(cur_tasks);
//
//	start_async_write_of_executed_tasks(0);
//
//	wait_till_executed_tasks_transfer_completion(0);
//
//	DMSG("--------------------------------------------------------------------------\n");
//	}
//
//	DMSG("Out of the loop\n");

//		int prev_state = -1, cur_state = 0, next_state = 1;
//		int num_tasks = 0;
//
////		//DMSG("Started recving ready task cur state\n");
//		start_async_read_of_ready_tasks(cur_state);
//
//		i = 1;
//		while(1){
//
//			//DMSG("--------------------------------------------------------------------------\n");
//			//DMSG("Started the loop prev_state = %d, cur_state = %d, next_state = %d\n", prev_state, cur_state,
//					next_state);
//
//			wait_till_ready_tasks_transfer_completion(cur_state);
//
//			start_async_read_of_ready_tasks(next_state);
//
//			cur_tasks = deseralize_tasks(cur_state, &num_tasks);
//
//			if(cur_tasks != NULL)
//				execute_tasks(cur_tasks, num_tasks, nb_threads);
//
//			//DMSG("Completed the execution of current state tasks\n ");
//
//			wait_till_executed_tasks_transfer_completion(prev_state);
//
//			start_async_write_of_executed_tasks(cur_state);
//
//			prev_state = cur_state;
//			cur_state = next_state;
//			next_state =  (next_state+1)%PIPELINE_DEPTH;
//
//			i--;
//			if(!i){
//                break;
//			}
//		}

			DMSG("Started receiving ready task cur state\n");
			start_async_read_of_ready_tasks(0, cluster_id);

			wsr_task_deseralize_tasks(0, 0, nb_threads);

//			WSR_TASK_P init_task = wsr_create_deseralize_task(0);
//			wsr_add_task_to_cdeque(init_task, 0);

			//Launch all the threads
			//start compute cluster threads
			int res = -1;
			pthread_t t[nb_threads];
			int param[nb_threads];
			for(i=1;i<nb_threads; i++){
				param[i] = i;
				while(1) {
					res = pthread_create(&t[i], NULL, wsr_cdeque_execute, &param[i]);
					if( res == 0) {
						break;
					}
					if( res == -EAGAIN ){
						usleep(100000);
					} else {
						EMSG("pthread_create failed i %d, res = %d\n", i, res);
						exit(-1);
					}
				}
				//DMSG("pthread create launched thread %d, locally called %d\n", (int) t[i], i);
			}

			int parm = 0;
			wsr_cdeque_execute(&parm);

			int ret_code = -1;
			void *ret;
			for (i = 1; i < nb_threads; i++) {
				pthread_join(t[i], &ret);

				ret_code = ((int *)ret)[0];
		//		if(ret_code != 0){
		//			EMSG("pthread return code for %d failed\n", i);
		//			exit(-1);
		//		}
			}



	//DMSG("Exited the loop \n");
	for(i=0;i<PIPELINE_DEPTH;i++)
		free(buf[i]);

	for(i=0;i<PIPELINE_DEPTH;i++)
		mppa_close(io_to_cc_fd[i]);

	for(i=0;i<PIPELINE_DEPTH;i++)
		mppa_close(cc_to_io_fd[i]);

	mppa_close_barrier(sync_io_to_cc_fd, sync_cc_to_io_fd);

	mppa_tracepoint(wsr, cc_main__out, cluster_id);

	mppa_exit(0);

}


