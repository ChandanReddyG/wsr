/*
 * wsr_io_thread.c
 *
 *
 *      Author: accesscore
 */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/wait.h>
#include <errno.h>

#include <mppa_bsp.h>
#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "wsr_task.h"
#include "wsr_seralize.h"
#include "wsr_task_functions.h"

#define ALIGN_MATRIX 1024 * 8

// Number of clusters
static unsigned long nb_clusters;
// Number of threads per cluster
static unsigned long nb_threads;

static const char *nb_threads_str;
static const char *nb_clusters_str;

static int cc_to_io_fd[BSP_NB_CLUSTER_MAX][PIPELINE_DEPTH], io_to_cc_fd[BSP_NB_CLUSTER_MAX][PIPELINE_DEPTH];

static mppa_aiocb_t  io_to_cc_aiocb[BSP_NB_CLUSTER_MAX][PIPELINE_DEPTH], cc_to_io_aiocb[BSP_NB_CLUSTER_MAX][PIPELINE_DEPTH];

int sync_io_to_cc_fd = -1;
int sync_cc_to_io_fd = -1;

// Initialization of the barrier (must be called before mppa_spawn)
static int
mppa_init_barrier(const char *sync_io_to_clusters_path,
		const char *sync_clusters_to_io_path,
		int *sync_io_to_cluster_fd,
		int *sync_clusters_to_io_fd)
{
	// Open IO to Clusters sync connector
	*sync_io_to_cluster_fd = mppa_open(sync_io_to_clusters_path, O_WRONLY);
	if (*sync_io_to_cluster_fd < 0) {
		EMSG("Opening %s failed!\n", sync_io_to_clusters_path);
		return 1;
	}
	int ranks[BSP_NB_CLUSTER_MAX];
	for (int i = 0; i < nb_clusters; ++i) ranks[i] = i;

	if (mppa_ioctl(*sync_io_to_cluster_fd, MPPA_TX_SET_RX_RANKS, nb_clusters, ranks) < 0) {
		EMSG("ioctl MPPA_TX_SET_RX_RANK on sync_io_to_cluster_fd failed!\n");
		return 1;
	}

	// Open Clusters to IO sync connector
	*sync_clusters_to_io_fd = mppa_open(sync_clusters_to_io_path, O_RDONLY);
	if (*sync_clusters_to_io_fd < 0) {
		EMSG("Opening %s failed!\n", sync_clusters_to_io_path);
		return 1;
	}
	unsigned long long match = ~((0x1ULL << nb_clusters) - 1);
	if (mppa_ioctl(*sync_clusters_to_io_fd, MPPA_RX_SET_MATCH, match) < 0) {
		EMSG("ioctl MPPA_RX_SET_MATCH failed on %s\n", sync_clusters_to_io_path);
		return 1;
	}

	return 0;

}

// Barrier between IO Cluster and all Clusters
static int
mppa_barrier(int sync_io_to_cluster_fd, int sync_clusters_to_io_fd)
{
	DMSG("[Cluster I/O] mppa_barrier...");

	unsigned long long dummy;
	if (mppa_read(sync_clusters_to_io_fd, &dummy, sizeof(dummy)) != sizeof(unsigned long long)) {
		EMSG("Read sync_clusters_to_io_fd failed!\n");
		return 1;
	}
	unsigned long long mask = 1LL;
	if (mppa_write(sync_io_to_cluster_fd, &mask, sizeof(mask)) != sizeof(mask)) {
		EMSG("write sync_io_to_cluster_fd failed!\n");
		return 1;
	}
	DMSG(" done!\n");

	return 0;

}

// Close barrier connectors
static void
mppa_close_barrier(int sync_io_to_cluster_fd, int sync_clusters_to_io_fd)
{
	mppa_close(sync_clusters_to_io_fd);
	mppa_close(sync_io_to_cluster_fd);
}

mppa_aiocb_t temp;

void start_async_write_of_ready_tasks(int cluster_id, int state, char *buf, int size ){


	if(state<0 || state >= PIPELINE_DEPTH)
		return;

	//		state  = 0;
	int portal_fd = io_to_cc_fd[cluster_id][state];
	mppa_aiocb_t *cur_aiocb = &io_to_cc_aiocb[cluster_id][state];
	mppa_aiocb_ctor(cur_aiocb, portal_fd, buf, size);
	mppa_aiocb_set_pwrite(cur_aiocb, buf, size, 0);
	int status = mppa_aio_write(cur_aiocb);
	assert(status == 0);
	DMSG("Starting the async write of ready task for cur_state = %d , ret = %d \n", state, status);
	return;

}

void wait_till_ready_task_transfer_completion(int cluster_id, int state, int size){

	DMSG("waiting for the ready task transfer to complete for state = %d\n", state);

	if(state<0 || state >= PIPELINE_DEPTH)
		return;

	//		state = 0;

	mppa_aiocb_t *cur_aiocb = &io_to_cc_aiocb[cluster_id][state];
	int status = mppa_aio_wait(cur_aiocb);
	//	assert(status == size);

	DMSG(" the ready task transfer is complete for state = %d, ret = %d\n", state, status);
	return;

}

void start_async_read_of_executed_tasks(int cluster_id, int state, char *buf, int size ){


	if(state<0 || state >= PIPELINE_DEPTH)
		return;

	//		state = 0;

	int portal_fd = cc_to_io_fd[cluster_id][state];
	mppa_aiocb_t *cur_aiocb = &cc_to_io_aiocb[cluster_id][state];

	mppa_aiocb_ctor(cur_aiocb, portal_fd, buf, size);
	mppa_aiocb_set_trigger(cur_aiocb, 1);
	int status = mppa_aio_read(cur_aiocb);
	assert(status == 0);
	DMSG("Starting the async read of executed task for cur_state = %d, ret = %d \n", state, status);
	return;

}

void wait_till_executed_task_transfer_completion(int cluster_id, int state, int size){

	DMSG("waiting for the executed task transfer to complete for state = %d\n", state);
	if(state<0 || state >= PIPELINE_DEPTH)
		return;

	//		state = 0;
	mppa_aiocb_t *cur_aiocb = &cc_to_io_aiocb[cluster_id][state];

	int status =  mppa_aio_wait(cur_aiocb);
	assert(status == size);
	DMSG(" the executed task transfer is complete for state = %d, ret = %d\n", state, status);
	return;

}

void *service_cc(void *arg){

	int cluster_id = ((int *)arg)[0];

	DMSG("CC thread started = %d\n", cluster_id);
	int ret = -1;
	char *buf[PIPELINE_DEPTH];
	int i = 0;
	for(i=0;i<PIPELINE_DEPTH;i++){
		posix_memalign((void**)&(buf[i]), ALIGN_MATRIX, BUFFER_SIZE);
		if (!buf[i]) {
			EMSG("Memory allocation failed: \n");
			mppa_exit(1);
		}
	}

	int size;
	//	for(i=0;i<1;i++){



//		for(i=0;i<3;i++){
//		start_async_read_of_executed_tasks(cluster_id, 0, buf[1], BUFFER_SIZE);
//		//Select
//		WSR_TASK_LIST_P task_list = get_next_task_list(cluster_id);
//
//		if(task_list != NULL)
//			size = wsr_serialize_tasks(task_list, buf[0]);
//
//		start_async_write_of_ready_tasks(cluster_id, 0, buf[0], size);
//
//		wait_till_ready_task_transfer_completion(cluster_id, 0, size);
//
//		wait_till_executed_task_transfer_completion(cluster_id, 0, BUFFER_SIZE);
//
//		WSR_TASK_LIST_P c = wsr_deseralize_tasks(buf[1], size);
//		}
//
//		DMSG("Out of the loop\n");

	int prev_state = -1, cur_state = 0, next_state = 1;


	i = 1;
	while(1){

		DMSG("--------------------------------------------------------------------------\n");
		DMSG("Started the loop  prev_state = %d, cur_state = %d, next_state = %d\n", prev_state, cur_state,
				next_state);

		//Receive the completed tasks of prev state
		if(prev_state>-1){
			wait_till_executed_task_transfer_completion(cluster_id, prev_state, BUFFER_SIZE);

			i--;
			if(!i)
				break;
		}
		start_async_read_of_executed_tasks(cluster_id, cur_state , buf[cur_state],BUFFER_SIZE);

		//Start selection of next tasks
		WSR_TASK_LIST_P task_list = get_next_task_list(cluster_id);

		if(task_list == NULL)
			DMSG("task_list is null\n");

		size = wsr_serialize_tasks(task_list, buf[cur_state]);

		if(prev_state>-1){
			wait_till_ready_task_transfer_completion(cluster_id, prev_state, size);
		}

		start_async_write_of_ready_tasks(cluster_id, cur_state, buf[cur_state], size);

		if(task_list == NULL)
			break;

		prev_state = cur_state;
		cur_state = next_state;
		next_state =  (next_state + 1)%3;
	}


	//Verify the output


	ret = 1;
	pthread_exit((void *)&ret);
	return NULL;
}


int main(int argc, char **argv) {
	/////////////////////////////
	// Get arguments from host //
	/////////////////////////////

	DMSG("IO cluster has started\n");
	if (argc != 3) {
		EMSG("error in arguments\n");
		mppa_exit(1);
	}



	int argn = 1;
	nb_clusters_str = argv[argn++];
	nb_threads_str = argv[argn++];
	nb_clusters = convert_str_to_ul(nb_clusters_str);
	nb_threads = convert_str_to_ul(nb_threads_str);

	DMSG("Number of clusters = %lu, number of threads = %lu \n", nb_clusters, nb_threads);

	int i = 0, j =0, k = 0;
	int io_dnoc_rx_port = 7;
	int cluster_dnoc_rx_port = 1;
	int io_cnoc_rx_port = 1;
	int cluster_cnoc_rx_port = 1;

	char io_to_cc_path[nb_clusters][PIPELINE_DEPTH][128];
	char cc_to_io_path[nb_clusters][PIPELINE_DEPTH][128];
	for(k=0;k<nb_clusters;k++){
	for(i=0;i<PIPELINE_DEPTH;i++){
		snprintf(io_to_cc_path[k][i], 128, "/mppa/portal/%d:%d", k, cluster_dnoc_rx_port++);
        snprintf(cc_to_io_path[k][i], 128, "/mppa/portal/%d:%d", mppa_getpid() + k%BSP_NB_DMA_IO, io_dnoc_rx_port++);
	}


	}

//	for(j=0;j<PIPELINE_DEPTH;j++){
//		for (int i = 0; i < BSP_NB_DMA_IO; i++) {
//			snprintf(cc_to_io_path[j][i], 128, "/mppa/portal/%d:%d", mppa_getpid() + i, io_dnoc_rx_port++);
//		}
//	}

	char sync_io_to_cc_path[128];
	snprintf(sync_io_to_cc_path, 128, "/mppa/sync/[0..%lu]:%d", nb_clusters - 1, cluster_cnoc_rx_port++);
	char sync_cc_to_io_path[128];
	snprintf(sync_cc_to_io_path, 128, "/mppa/sync/%d:%d", mppa_getpid(), io_cnoc_rx_port++);

	DMSG("Opening io to cc portals\n");

	//Opening portal from io to all compute clusters and vice versa
	for (int rank = 0; rank < nb_clusters; rank++) {
		for(i =0;i<PIPELINE_DEPTH;i++){
			// Open a multicast portal to send task groups
			DMSG("Open portal %s\n", io_to_cc_path[rank][i]);
			if((io_to_cc_fd[rank][i] = mppa_open(io_to_cc_path[rank][i], O_WRONLY)) < 0){
				EMSG("Failed to open io_to_cc_portal %d to rank = %d \n", i, rank  );
				mppa_exit(1);
			}

			// Set unicast target 'rank'
//			if (mppa_ioctl(io_to_cc_fd[rank][i], MPPA_TX_SET_RX_RANK, rank) < 0) {
//				EMSG("Preparing multiplex %d failed!\n", rank);
//				mppa_exit(1);
//			}

			// Select which interface 'fd' will use to send task groups  to 'rank'
			// We will use mppa_aio_write to perform the transfer: mppa_aio_write automatically select and reserve HW
			// resources at each call. It select between the [4 IO DMA interfaces] * [8 DMA resources] = 32 DMA resources
			// MPPA_TX_SET_IFACE ioctl force mppa_aio_write for this fd to select a DMA resource in the given interface
			// 'rank % BSP_NB_DMA_IO' policy ensure that transfers on one interface don't interfere with transfers of other
			// interfaces.
			if (mppa_ioctl(io_to_cc_fd[rank][i], MPPA_TX_SET_IFACE, rank % BSP_NB_DMA_IO) < 0) {
				EMSG("Set iface %d failed!\n", rank % BSP_NB_DMA_IO);
				mppa_exit(1);
			}

			// mppa_aio_write needs some HW resources to perform the transfer. If no hardware resource are currently
			// available, default behavior of mppa_aio_write is to return -EAGAIN. In this case, safe blocking call of this
			// function should be done like:
			// while ( ( res = mppa_aio_write(&aiocb) ) == -EAGAIN ) {}
			// if ( res < 0 ) ...
			// Going in and out of MPPAIPC continuously may disturb the system.
			// With MPPA_TX_WAIT_RESOURCE_ON ioctl, further call of mppa_aio_write on this fd will block until an hardware
			// resource is available.
			if (mppa_ioctl(io_to_cc_fd[rank][i], MPPA_TX_WAIT_RESOURCE_ON, 0) < 0) {
				EMSG("Preparing wait resource %d failed!\n", rank);
				mppa_exit(1);
			}

		}

		DMSG("Opening cc to io portals\n");
		//open portals to transfer tasks from  cc to io
		for(i =0;i<PIPELINE_DEPTH;i++){
			DMSG("Open portal %s\n", cc_to_io_path[rank][i]);
			if((cc_to_io_fd[rank][i] = mppa_open(cc_to_io_path[rank][i], O_RDONLY)) < 0){
					EMSG("Failed to open io_to_cc_portal %d to rank = %d \n", i, rank  );
					mppa_exit(1);
				}
		}

	}

	// sync connector Clusters->IO (RD) MUST be opened before any cluster attempt to write in.
	// Then, we open it before the spawn.
	DMSG("Opening barriers \n");

	if (mppa_init_barrier(sync_io_to_cc_path, sync_cc_to_io_path, &sync_io_to_cc_fd,
			&sync_cc_to_io_fd)) {
		EMSG("mppa_init_barrier failed\n");
		mppa_exit(1);
	}

	// Preload Cluster binary to all clusters (hardware broadcast)
	mppa_pid_t pids[nb_clusters];
	unsigned int nodes[nb_clusters];
	for (int i = 0; i < nb_clusters; i++) {
		nodes[i] = i;
	}
	if (mppa_preload(CLUSTER_BIN_NAME, nb_clusters, nodes) < 0) {
		EMSG("preload failed\n");
		mppa_exit(1);
	}

	DMSG(" Spawn all compute clusters\n");
	for (int i = 0; i < nb_clusters; i++) {
		// [i%BSP_NB_DMA_IO] ensures independence between the group of clusters
		const char *_argv[] = { CLUSTER_BIN_NAME, io_to_cc_path[i][0], io_to_cc_path[i][1], io_to_cc_path[i][2],
				cc_to_io_path[i][0],cc_to_io_path[i][1],cc_to_io_path[i][2],
				sync_io_to_cc_path, sync_cc_to_io_path,
				nb_clusters_str, nb_threads_str, 0 };
		if ((pids[i] = mppa_spawn(i, NULL, CLUSTER_BIN_NAME, _argv, NULL)) < 0) {
			EMSG("spawn cluster %d failed, ret = %d\n", i, pids[i]);
			mppa_exit(1);
		}
		DMSG("%d spawned : %d\n", i, pids[i]);
	}

	// Synchronization between all Clusters and IO
	if (mppa_barrier(sync_io_to_cc_fd, sync_cc_to_io_fd)) {
		EMSG("mppa_barrier failed\n");
		mppa_exit(1);
	}

	DMSG("Sync complete\n Starting computation\n");

	pthread_t t[nb_clusters];
	int param[nb_clusters];

	//start threads to service cc clusters
	int res = -1;
	for(i=0;i<nb_clusters ; i++){
		param[i] = i;
		while(1) {
			res = pthread_create(&t[i], NULL, service_cc, &param[i]);
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

	int ret_code = -1;
	void *ret;
	for (i = 0; i < nb_clusters; i++) {
		pthread_join(t[i], &ret);
//		if (pthread_join(t[i], &ret) != 0) {
//			EMSG("pthread_join for thread %d failed\n", i);
//			exit(-1);
//		}
		ret_code = ((int *)ret)[0];
		if(ret_code != 1){
			EMSG("pthread return code for %d failed\n", i);
			exit(-1);
		}
	}

	DMSG("Computation complete\n");
	mppa_close_barrier(sync_io_to_cc_fd, sync_cc_to_io_fd);
	for (int i = 0; i < nb_clusters; i++) {
		int status;
		int ret_pid = mppa_waitpid(pids[i], &status, 0);
		if ((ret_pid != pids[i]) ||     /* Waitpid error               ? */
				!WIFEXITED(status) ||   /* Does it exited normally     ? */
				(WEXITSTATUS(status) != 0)) { /* Does it exited with success ? */
			EMSG("Error with Cluster %d exit\n ", pids[i]);
			mppa_exit(1);;
		}
	}

}

