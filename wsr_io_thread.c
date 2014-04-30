/*
 * wsr_io_thread.c
 *
 *
 *      Author: accesscore
 */
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

#include <mppa_bsp.h>
#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "wsr_task.h"
#include "wsr_seralize.h"

// Number of clusters
static unsigned long nb_clusters;
// Number of threads per cluster
static unsigned long nb_threads;

static const char *nb_threads_str;
static const char *nb_clusters_str;

typedef struct{
	//fd returned by mppa_open
	int fd;

	// aiocb set by mppa_aio_ctor and used by mppa_aio_write and mppa_aio_rearm
	mppa_aiocb_t cb;
}cluster_portal_t;

typedef struct {
	cluster_portal_t	p[PIPELINE_DEPTH];
} cluster_portals_t;

cluster_portals_t io_to_cc_cluster_portal[BSP_NB_CLUSTER_MAX];

//size of buffers used in double buffering scheme: 0.5 MB
static void* buf[PIPELINE_DEPTH];


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

int main(int argc, char **argv) {
	/////////////////////////////
	// Get arguments from host //
	/////////////////////////////

	if (argc != 3) {
		EMSG("error in arguments\n");
		mppa_exit(1);
	}

	int argn = 1;
	nb_clusters_str = argv[argn++];
	nb_threads_str = argv[argn++];
	nb_clusters = convert_str_to_ul(nb_clusters_str);
	nb_threads = convert_str_to_ul(nb_threads_str);

	int i = 0, j =0;
	int io_dnoc_rx_port = 1;
	int cluster_dnoc_rx_port = 1;
	int io_cnoc_rx_port = 1;
	int cluster_cnoc_rx_port = 1;

	char io_to_cc_path[PIPELINE_DEPTH][128];
	for(i=0;i<PIPELINE_DEPTH;i++)
        snprintf(io_to_cc_path[i], 128, "/mppa/portal/[0..%lu]:%d", nb_clusters - 1, cluster_dnoc_rx_port++);

	char cc_to_io_path[PIPELINE_DEPTH][BSP_NB_DMA_IO][128];
	for(j=0;j<PIPELINE_DEPTH;j++){
        for (int i = 0; i < BSP_NB_DMA_IO; i++) {
                snprintf(cc_to_io_path[j][i], 128, "/mppa/portal/%d:%d", mppa_getpid() + i, io_dnoc_rx_port++);
        }
	}

	char sync_io_to_cc_path[128];
	snprintf(sync_io_to_cc_path, 128, "/mppa/sync/[0..%lu]:%d", nb_clusters - 1, cluster_cnoc_rx_port++);
	char sync_cc_to_io_path[128];
	snprintf(sync_cc_to_io_path, 128, "/mppa/sync/%d:%d", mppa_getpid(), io_cnoc_rx_port++);

	//Opening the portal for receiving completed tasks
	mppa_aiocb_t aiocb_cc_to_io[PIPELINE_DEPTH][BSP_NB_DMA_IO];
	int cc_to_io_fd[PIPELINE_DEPTH][BSP_NB_DMA_IO];

	for(j=0;j<PIPELINE_DEPTH;j++){
        for (int i = 0; i < BSP_NB_DMA_IO; i++) {
                cc_to_io_fd[j][i] = mppa_open(cc_to_io_path[j][i], O_RDONLY);
                mppa_aiocb_ctor(&aiocb_cc_to_io[j][i], cc_to_io_fd[j][i], buf[j], BUFFER_SIZE);
                mppa_aiocb_set_trigger(&aiocb_cc_to_io[j][i], nb_clusters / BSP_NB_DMA_IO);
                if (mppa_aio_read(&aiocb_cc_to_io[j][i]) < 0) {
                        EMSG("Error while aio_read matrix C\n");
                        mppa_exit(1);
                }
        }
	}

	//Opening portal from io to all the clusters
	for (int rank = 0; rank < nb_clusters; rank++) {
		for(i =0;i<PIPELINE_DEPTH;i++){
		   // Open a multicast portal to send task groups
			io_to_cc_cluster_portal[rank].p[i] = mppa_open(io_to_cc_path[i], O_WRONLY);

			// Set unicast target 'rank'
			if (mppa_ioctl(io_to_cc_cluster_portal[rank].p[i].fd, MPPA_TX_SET_RX_RANK, rank) < 0) {
				EMSG("Preparing multiplex %d failed!\n", rank);
				mppa_exit(1);
			}

			// Select which interface 'fd' will use to send task groups  to 'rank'
			// We will use mppa_aio_write to perform the transfer: mppa_aio_write automatically select and reserve HW
			// resources at each call. It select between the [4 IO DMA interfaces] * [8 DMA resources] = 32 DMA resources
			// MPPA_TX_SET_IFACE ioctl force mppa_aio_write for this fd to select a DMA resource in the given interface
			// 'rank % BSP_NB_DMA_IO' policy ensure that transfers on one interface don't interfere with transfers of other
			// interfaces.
			if (mppa_ioctl(io_to_cc_cluster_portal[rank].p[i].fd, MPPA_TX_SET_IFACE, rank % BSP_NB_DMA_IO) < 0) {
				EMSG("Set iface %d failed!\n", rank % BSP_NB_DMA_IO);
				mppa_exit(1);
			}

			// mppa_aio_write needs some HW resources to perform the transfer. If no hardware resource are currently
			// available, default behavior of mppa_aio_write is to return -EAGAIN. In this case, safe blocking call of this
			// function shoule be done like:
			// while ( ( res = mppa_aio_write(&aiocb) ) == -EAGAIN ) {}
			// if ( res < 0 ) ...
			// Going in and out of MPPAIPC continuously may disturb the system.
			// With MPPA_TX_WAIT_RESOURCE_ON ioctl, further call of mppa_aio_write on this fd will block until an hardware
			// resource is available.
			if (mppa_ioctl(io_to_cc_cluster_portal[rank].p[i].fd, MPPA_TX_WAIT_RESOURCE_ON, 0) < 0) {
				EMSG("Preparing wait resource %d failed!\n", rank);
				mppa_exit(1);
			}
			mppa_aiocb_ctor(&io_to_cc_cluster_portal[rank].p[i].cb, io_to_cc_cluster_portal[rank].p[i].fd, buf[i], BUFFER_SIZE);

		}

	}

	// sync connector Clusters->IO (RD) MUST be opened before any cluster attempt to write in.
	// Then, we open it before the spawn.
	int sync_io_to_cc_fd = -1;
	int sync_cc_to_io_fd = -1;
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

	// Spawn (start) all clusters
	for (int i = 0; i < nb_clusters; i++) {
		// [i%BSP_NB_DMA_IO] ensures independence between the group of clusters
		const char *_argv[] = { CLUSTER_BIN_NAME, io_to_cc_path[0], io_to_cc_path[1], io_to_cc_path[2],
					cc_to_io_path[0][i % BSP_NB_DMA_IO],cc_to_io_path[1][i % BSP_NB_DMA_IO],cc_to_io_path[1][i % BSP_NB_DMA_IO],
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

	//main loop


	mppa_close_barrier(sync_io_to_cc_fd, sync_cc_to_io_fd);
	for (int i = 0; i < nb_clusters; i++) {
		int status;
		int ret_pid = mppa_waitpid(pids[i], &status, 0);
		if ((ret_pid != pids[i]) ||     /* Waitpid error               ? */
			!WIFEXITED(status) ||   /* Does it exited normally     ? */
			(WEXITSTATUS(status) != 0)) { /* Does it exited with success ? */
			EMSG("Error with Cluster %d exit\n ", pids[i]);
			int ret = EXIT_FAILURE;
			mppa_exit(1);;
		}
	}


}

