/*
 * Copyright (C) 2013 Kalray SA. All rights reserved.
 * This code is Kalray proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of Kalray SA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <mppa_bsp.h>
#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "tuto_db.h"
#include "tuto_db_trace.h"

#define ALIGN_MATRIX 1024 * 8

#ifndef __k1__
#  define BSP_NB_DMA_IO 4
#endif

//////////////////////////////////
// Global variables declaration //
//////////////////////////////////

// Width of global matrix in DDR
static unsigned long global_n;
// Heigth of global matrix in DDR
static unsigned long global_m;
// Heigth of submatrices in cluster memory (cluster width == global width)
static unsigned long cluster_m;
// Number of clusters
static unsigned long nb_clusters;
// Number of threads per cluster
static unsigned long nb_threads;

// String representation of global variables (we keep them between main(argv) and mppa_spawn)
static const char *global_n_str;
static const char *global_m_str;
static const char *cluster_m_str;
static const char *nb_threads_str;
static const char *nb_clusters_str;

// Global matrix size in Bytes
static unsigned long long global_matrix_size;
// Cluster submatrix size in Bytes
static unsigned long long cluster_matrix_size;

// Pointer to matrix a
static tuto_db_t *a;
// Pointer to matrix b
static tuto_db_t *b;
// Pointer to result matrix
static tuto_db_t *matres;

// Structure for asynchronuous write
// Current MPPAIPC limitation force the usage of one fd per 'flying' asynchronuous write transfer.
// Then, we also need for one aiocb per 'flying' transfer.
// Finally, we open one portal (WR) per cluster per submatrix (a,b) == nb portals = nb clusters * 2
typedef struct {
	// fd returned by mppa_open
	int fd;
	// aiocb set by mppa_aio_ctor and used by mppa_aio_write and mppa_aio_rearm
	mppa_aiocb_t cb;
	// before each mppa_aio_write, we must ensure that previous async transfer with this aiocb is finished calling
	// mppa_aio_wait or mppa_aio_rearm function.
	// !! Except for first transfer !!
	int first;
} cluster_portal_t;

// Structure containing both cluster_portal_t for one cluster
typedef struct {
	cluster_portal_t	a;
	cluster_portal_t	b;
} cluster_portals_t;

// Array containing all (nb_clusters*2) cluster_portal_t structures
cluster_portals_t cluster_portals[BSP_NB_CLUSTER_MAX];

/////////////////////////////////////////////
// Synchronization IO / Clusters functions //
/////////////////////////////////////////////

// These functions are used to synchronize all clusters after spawn
// They may be used for further global synchronization

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

// This function sends a submatrix to a given cluster
// It includes the wait step for the previous asynchronuous transfer on the given fd/aiocb.
// rank is only used for debug. portals in cluster_portal_t *current_portal are already set to send to wanted cluster.
static void
send_submatrix(cluster_portal_t *current_portal, tuto_db_t *_current_matrix, int y, int rank __attribute__((unused)))
{
	// matrix a or b base
	tuto_db_t (*current_matrix)[global_n] = (tuto_db_t (*)[global_n])_current_matrix;

	// Base of the submatrix (according to y)
	void *base_submat = &current_matrix[y];
	DMSG("IO is sending mat[%d] to %d\n", y, rank);

	// Configure aiocb
	mppa_aiocb_set_pwrite (&(current_portal->cb), base_submat, cluster_matrix_size, 0);

	// Except for first send, wait for the end of the last transfer to this requester of this matrix
	if (current_portal->first == 1) {
		mppa_tracepoint(tuto_db, wait_previous_transfer_end__in, rank);
		if (mppa_aio_wait(&(current_portal->cb)) < 0) {
			EMSG("Error while waiting submatrix send\n");
			mppa_exit(1);
		}
		mppa_tracepoint(tuto_db, wait_previous_transfer_end__out);
	} else
		current_portal->first = 1;

	mppa_tracepoint(tuto_db, wait_ucore_resource__in, rank);
	int ret;
    int cpt = __k1_read_dsu_timestamp();
	if ((ret = mppa_aio_write(&current_portal->cb)) < 0) {
		EMSG("Error while sending submatrix\n");
		mppa_exit(1);
	}
    cpt = __k1_read_dsu_timestamp() - cpt;
    printf("Async Transfer start time = %d\n", cpt);

	mppa_tracepoint(tuto_db, wait_ucore_resource__out);
}

// remote server for sending submatrices.
// the queue is actively polled. A message queue contains all needed information to send 2 submatrices (a and b)
static void
submat_server(int getsub_queue_fd)
{
	int ret;
	int nb_cluster_finished = 0;
	while (nb_cluster_finished < nb_clusters) {
		matrix_getsub_t matrix_getsub;
		// Get last submatrix request (FIFO)
        int queue_size = -1;
        int cpt = __k1_read_dsu_timestamp();
        ret = mppa_ioctl(getsub_queue_fd, MPPA_RX_GET_COUNTER, &queue_size);
        cpt = __k1_read_dsu_timestamp() - cpt;
         
        printf("queue size = %d, Time taken = %d\n", queue_size, cpt);

		if ((ret = mppa_read(getsub_queue_fd, &matrix_getsub, sizeof(matrix_getsub_t))) < 0) {
			EMSG("getsub_queue read error: %d\n", ret);
			mppa_exit(1);
		}

		// When a cluster have received/compute/send_result all its submatrices, it sends a message with rank = -1
		if (matrix_getsub.rank < 0) {
			mppa_tracepoint(tuto_db, one_cluster_finished);
			nb_cluster_finished++;
			continue;
		}

		// Get the aiocb set for the current cluster
		// That's why we don't need matrix_getsub.rank in send_submatrix() function
		cluster_portals_t *current_portals = &cluster_portals[matrix_getsub.rank];
		// Send submatrices a and b to cluster rank
		send_submatrix(&current_portals->a, (tuto_db_t*)a, matrix_getsub.ya, matrix_getsub.rank);
		send_submatrix(&current_portals->b, (tuto_db_t*)b, matrix_getsub.yb, matrix_getsub.rank);
	}
}

int
main(int argc, char **argv)
{

	mppa_tracepoint(tuto_db, main__in);
	int ret;

	/////////////////////////////
	// Get arguments from host //
	/////////////////////////////

	if (argc != 9) {
		EMSG("error in arguments\n");
		mppa_exit(1);
	}

	int argn = 1;
	global_n_str = argv[argn++];
	global_m_str = argv[argn++];
	cluster_m_str = argv[argn++];
	nb_clusters_str = argv[argn++];
	nb_threads_str = argv[argn++];

	const char *a_buffer_path = argv[argn++];
	const char *b_buffer_path = argv[argn++];
	const char *r_buffer_path = argv[argn++];

	global_n = convert_str_to_ul(global_n_str);
	global_m = convert_str_to_ul(global_m_str);
	cluster_m = convert_str_to_ul(cluster_m_str);
	nb_clusters = convert_str_to_ul(nb_clusters_str);
	nb_threads = convert_str_to_ul(nb_threads_str);

	global_matrix_size = global_n * global_m * sizeof(tuto_db_t);
	cluster_matrix_size = global_n * cluster_m * sizeof(tuto_db_t);

	// Store runtime information at the head of DSU trace
	mppa_tracepoint(tuto_db, info, "global_n", global_n);
	mppa_tracepoint(tuto_db, info, "global_m", global_m);
	mppa_tracepoint(tuto_db, info, "cluster_m", cluster_m);
	mppa_tracepoint(tuto_db, info, "nb_clusters", nb_clusters);
	mppa_tracepoint(tuto_db, info, "nb_threads", nb_threads);

	////////////////////////////
	// Allocation of matrices //
	////////////////////////////

	// Alignement is to ensure good performances of DMA (DDR pages)
	posix_memalign((void**)&matres, ALIGN_MATRIX, global_matrix_size);
	posix_memalign((void**)&a, ALIGN_MATRIX, global_matrix_size);
	posix_memalign((void**)&b, ALIGN_MATRIX, global_matrix_size);

	if (!a || !b || !matres) {
		EMSG("malloc failed\n");
		mppa_exit(1);
	}

	///////////////////////////////////////
	// PCIe initialization and reception //
	///////////////////////////////////////

	DMSG("Receiving matrices from PCIe\n");

	mppa_tracepoint(tuto_db, init_pcie__in);

	int a_buffer_fd, b_buffer_fd, r_buffer_fd;
	a_buffer_fd = mppa_open(a_buffer_path, O_RDONLY);
	if (a_buffer_fd < 0) {
		EMSG("Failed to open a_buffer_fd\n");
		mppa_exit(1);
	}
	b_buffer_fd = mppa_open(b_buffer_path, O_RDONLY);
	if (b_buffer_fd < 0) {
		EMSG("Failed to open b_buffer_fd\n");
		mppa_exit(1);
	}
	r_buffer_fd = mppa_open(r_buffer_path, O_WRONLY);
	if (r_buffer_fd < 0) {
		EMSG("Failed to open r_buffer_fd\n");
		mppa_exit(1);
	}

	mppa_tracepoint(tuto_db, init_pcie__out);

	mppa_tracepoint(tuto_db, recv_pcie__in);
	ret = mppa_read(a_buffer_fd, a, global_matrix_size);
	if (ret != global_matrix_size) {
		EMSG("Failed to read %llu bytes from fd a\n", global_matrix_size);
		mppa_exit(1);
	}
	ret = mppa_read(b_buffer_fd, b, global_matrix_size);
	if (ret != global_matrix_size) {
		EMSG("Failed to read %llu bytes from fd b\n", global_matrix_size);
		mppa_exit(1);
	}
	mppa_tracepoint(tuto_db, recv_pcie__out);

	print_matrix("Initial matrix a", global_n, global_m, a);
	print_matrix("Initial matrix b", global_n, global_m, b);

	//////////////////////////////////////
	// NoCIPC connectors path defintion //
	//////////////////////////////////////

	mppa_tracepoint(tuto_db, init_nocipc__in);
	int io_dnoc_rx_port = 1;
	int cluster_dnoc_rx_port = 1;
	int io_cnoc_rx_port = 1;
	int cluster_cnoc_rx_port = 1;

	char matrix_a_out_path[128];
	snprintf(matrix_a_out_path, 128, "/mppa/portal/[0..%lu]:%d", nb_clusters - 1, cluster_dnoc_rx_port++);
	char matrix_b_out_path[128];
	snprintf(matrix_b_out_path, 128, "/mppa/portal/[0..%lu]:%d", nb_clusters - 1, cluster_dnoc_rx_port++);
	char matrix_c_in_path[BSP_NB_DMA_IO][128];
	for (int i = 0; i < BSP_NB_DMA_IO; i++) {
		snprintf(matrix_c_in_path[i], 128, "/mppa/portal/%d:%d", mppa_getpid() + i, io_dnoc_rx_port++);
	}

	char sync_io_to_clusters_path[128];
	snprintf(sync_io_to_clusters_path, 128, "/mppa/sync/[0..%lu]:%d", nb_clusters - 1, cluster_cnoc_rx_port++);
	char sync_clusters_to_io_path[128];
	snprintf(sync_clusters_to_io_path, 128, "/mppa/sync/%d:%d", mppa_getpid(), io_cnoc_rx_port++);

	char getsub_queue_path[128];
	snprintf(getsub_queue_path, 128, "/mppa/queue.%d/%d:%d/[0..%lu]:%d", (int)sizeof(matrix_getsub_t),
		mppa_getpid(), io_dnoc_rx_port++, nb_clusters - 1, cluster_cnoc_rx_port++);

	///////////////////////////////
	// NoCIPC connectors opening //
	///////////////////////////////

	// Opening queue which will be used to get submatrices requests and distribute them to requiring cluster
	int getsub_queue_fd = mppa_open(getsub_queue_path, O_RDONLY);

	// Opening portal receiving C result matrix
	// There is one receiving portal per IO Cluster DMA interface. Like for IO to Cluster transfer, we ensure that
	// Cluster to IO transfers on one interface do not distub other interfaces transfers.
	// Briefly, Clusters 0, 4, 8 and 12 'talk' with IO Cluster DMA interface 0, Clusters 1, 5, 9, 13 talk with IO
	// Cluster DMA interface 1 ...
	// Paths used for such connections do not share any NoC link bewteen group of clusters.
	mppa_aiocb_t aiocb_c_in[BSP_NB_DMA_IO];
	int matrix_c_in_fd[BSP_NB_DMA_IO];
	for (int i = 0; i < BSP_NB_DMA_IO; i++) {
		matrix_c_in_fd[i] = mppa_open(matrix_c_in_path[i], O_RDONLY);
		mppa_aiocb_ctor(&aiocb_c_in[i], matrix_c_in_fd[i], matres, global_matrix_size);
		mppa_aiocb_set_trigger(&aiocb_c_in[i], nb_clusters / BSP_NB_DMA_IO);
		if (mppa_aio_read(&aiocb_c_in[i]) < 0) {
			EMSG("Error while aio_read matrix C\n");
			mppa_exit(1);
		}
	}

	// Opening portal (WR) to submatrices a & b on all clusters
	// see cluster_portal_t comment
	for (int rank = 0; rank < nb_clusters; rank++) {
		// Open a multicast portal to send submatrices a: /mppa/portal/[0..nb_clusters]:X
		cluster_portals[rank].a.fd = mppa_open(matrix_a_out_path, O_WRONLY);
		// Set unicast target 'rank' for 'a.fd'
		if (mppa_ioctl(cluster_portals[rank].a.fd, MPPA_TX_SET_RX_RANK, rank) < 0) {
			EMSG("Preparing multiplex %d failed!\n", rank);
			mppa_exit(1);
		}
		// Select which interface 'a.fd' will use to send submatrices a to 'rank'
		// We will use mppa_aio_write to perform the transfer: mppa_aio_write automatically select and reserve HW
		// resources at each call. It select between the [4 IO DMA interfaces] * [8 DMA resources] = 32 DMA resources
		// MPPA_TX_SET_IFACE ioctl force mppa_aio_write for this fd to select a DMA resource in the given interface
		// 'rank % BSP_NB_DMA_IO' policy ensure that transfers on one interface don't interfere with transfers of other
		// interfaces.
		if (mppa_ioctl(cluster_portals[rank].a.fd, MPPA_TX_SET_IFACE, rank % BSP_NB_DMA_IO) < 0) {
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
		if (mppa_ioctl(cluster_portals[rank].a.fd, MPPA_TX_WAIT_RESOURCE_ON, 0) < 0) {
			EMSG("Preparing wait resource %d failed!\n", rank);
			mppa_exit(1);
		}
		mppa_aiocb_ctor(&cluster_portals[rank].a.cb, cluster_portals[rank].a.fd, a, cluster_matrix_size);

		// Same thing for b submatrices for 'rank'.
		cluster_portals[rank].b.fd = mppa_open(matrix_b_out_path, O_WRONLY);
		if (mppa_ioctl(cluster_portals[rank].b.fd, MPPA_TX_SET_RX_RANK, rank) < 0) {
			EMSG("Preparing multiplex %d failed!\n", rank);
			mppa_exit(1);
		}
		if (mppa_ioctl(cluster_portals[rank].b.fd, MPPA_TX_SET_IFACE, rank % BSP_NB_DMA_IO) < 0) {
			EMSG("Set iface %d failed!\n", rank % BSP_NB_DMA_IO);
			mppa_exit(1);
		}
		if (mppa_ioctl(cluster_portals[rank].b.fd, MPPA_TX_WAIT_RESOURCE_ON, 0) < 0) {
			EMSG("Preparing wait resource %d failed!\n", rank);
			mppa_exit(1);
		}
		mppa_aiocb_ctor(&cluster_portals[rank].b.cb, cluster_portals[rank].b.fd, b, cluster_matrix_size);
	}

	// sync connector Clusters->IO (RD) MUST be opened before any cluster attempt to write in.
	// Then, we open it before the spawn.
	int sync_io_to_cluster_fd = -1;
	int sync_clusters_to_io_fd = -1;
	if (mppa_init_barrier(sync_io_to_clusters_path, sync_clusters_to_io_path, &sync_io_to_cluster_fd,
		&sync_clusters_to_io_fd)) {
		EMSG("mppa_init_barrier failed\n");
		mppa_exit(1);
	}

	mppa_tracepoint(tuto_db, init_nocipc__out);

	////////////////////////
	// Spawn all clusters //
	////////////////////////

	mppa_tracepoint(tuto_db, spawn__in, nb_clusters);

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
		// matrix_c_in_path[i%BSP_NB_DMA_IO] ensures independance between the group of clusters
		const char *_argv[] = { CLUSTER_BIN_NAME, matrix_a_out_path, matrix_b_out_path,
					matrix_c_in_path[i % BSP_NB_DMA_IO], getsub_queue_path,
					sync_io_to_clusters_path, sync_clusters_to_io_path,
					global_n_str, global_m_str, cluster_m_str, nb_clusters_str, nb_threads_str, 0 };
		if ((pids[i] = mppa_spawn(i, NULL, CLUSTER_BIN_NAME, _argv, NULL)) < 0) {
			EMSG("spawn cluster %d failed, ret = %d\n", i, pids[i]);
			mppa_exit(1);
		}
		DMSG("%d spawned : %d\n", i, pids[i]);
	}
	mppa_tracepoint(tuto_db, spawn__out);

 #ifdef __k1__
	__k1_uint64_t start, end, compute_time;
	start = __k1_read_dsu_timestamp();
 #endif

	/////////////////////////////
	// Real stuff starts here: //
	/////////////////////////////

	// Synchronization between all Clusters and IO
	mppa_tracepoint(tuto_db, synchro__in);
	if (mppa_barrier(sync_io_to_cluster_fd, sync_clusters_to_io_fd)) {
		EMSG("mppa_barrier failed\n");
		mppa_exit(1);
	}
	mppa_tracepoint(tuto_db, synchro__out);

	// Set initial credits to clusters
	///!\ it must be done only once all clusters have open their own getsub_queue connector (WR)
	// Then, revious synchronization is mandatory.
	mppa_ioctl(getsub_queue_fd, MPPA_RX_SET_CREDITS, 1);

	// Start submatrices server
	mppa_tracepoint(tuto_db, submat_server__in);
	submat_server(getsub_queue_fd);
	mppa_tracepoint(tuto_db, submat_server__out);

	// Wait to receive all result submatrices in DDR
	mppa_tracepoint(tuto_db, wait_last_results__in);
	for (int i = 0; i < BSP_NB_DMA_IO; i++) {
		mppa_aio_wait(&aiocb_c_in[i]);
	}
	mppa_tracepoint(tuto_db, wait_last_results__out);

 #ifdef __k1__
	end = __k1_read_dsu_timestamp();
	compute_time = end - start;
	printf("%llu cycles\n", compute_time);
 #endif

	mppa_tracepoint(tuto_db, send_results_to_host__in);
	// Write result back in host memory
	if (mppa_pwrite(r_buffer_fd, matres, global_matrix_size, 0) != global_matrix_size) {
		EMSG("Failed to write %llu bytes from r_buffer_fd\n", global_matrix_size);
		mppa_exit(-1);
	}

	print_matrix("Final matrix c", global_n, global_m, matres);

	mppa_close_barrier(sync_io_to_cluster_fd, sync_clusters_to_io_fd);
	for (int i = 0; i < nb_clusters; i++) {
		int status;
		int ret_pid = mppa_waitpid(pids[i], &status, 0);
		if ((ret_pid != pids[i]) ||     /* Waitpid error               ? */
			!WIFEXITED(status) ||   /* Does it exited normally     ? */
			(WEXITSTATUS(status) != 0)) { /* Does it exited with success ? */
			EMSG("Error with Cluster %d exit\n ", pids[i]);
			ret = EXIT_FAILURE;
			mppa_exit(1);;
		}
	}
	mppa_tracepoint(tuto_db, send_results_to_host__out);

	mppa_tracepoint(tuto_db, main__out);
	mppa_exit(0);
}
