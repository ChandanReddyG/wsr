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
	DMSG("[Cluster %d] mppa_barrier...", mppa_getpid());
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

	DMSG("done\n");
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

	const char *sync_io_to_cc_path = argv[argn++];
	const char *sync_cc_to_io_path = argv[argn++];

	for(i=0;i<PIPELINE_DEPTH;i++){
		buf[i] = memalign(64, BUFFER_SIZE);
		if (!buf[i]) {
			EMSG("Memory allocation failed: \n");
			mppa_exit(1);
		}
	}
	int sync_io_to_cc_fd = -1;
	int sync_cc_to_io_fd = -1;
	if (mppa_init_barrier(sync_io_to_cc_path, sync_cc_to_io_path, &sync_io_to_cc_fd,
			&sync_cc_to_io_fd)) {
		EMSG("mppa_init_barrier failed\n");
		mppa_exit(1);
	}

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

	if (mppa_barrier(sync_io_to_cc_fd, sync_cc_to_io_fd)) {
		EMSG("mppa_barrier failed\n");
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

	for(i=0;i<PIPELINE_DEPTH;i++)
		free(buf[i]);

	for(i=0;i<PIPELINE_DEPTH;i++)
		mppa_close(io_to_cc_fd[i]);

	for(i=0;i<PIPELINE_DEPTH;i++)
		mppa_close(cc_to_io_fd[i]);

	mppa_close_barrier(sync_io_to_cc_fd, sync_cc_to_io_fd);

	mppa_exit(0);

}


