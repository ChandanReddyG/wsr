
#include <stdlib.h>
#include <malloc.h>

#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "wsr_task.h"

#include "tuto_db.h"
#include "tuto_db_trace.h"

#define IS_DEBUG 0

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

// locA, locB type -> tuto_db_t [PIPELINE_DEPTH][cluster_m][global_n]
// locA|locB points to the double buffered submatrix
// locA[0] points to 1st stage buffer tuto_db_t [cluster_m][global_n]
// locA[1] points to 2nd stage buffer tuto_db_t [cluster_m][global_n]
static tuto_db_t *locA;
static tuto_db_t *locB;

// curA, curB, locC type -> tuto_db_t [cluster_m][global_n]
// curA|curB points to the current already received buffer
static tuto_db_t *curA;
static tuto_db_t *curB;
// transA|transB points to the ongoing receiving buffer
static tuto_db_t *transA;
static tuto_db_t *transB;
// locC is the result matrix
static tuto_db_t *locC;


static int matrix_a_in_fd, matrix_b_in_fd, matrix_c_out_fd, queue_getsub_fd;
static mppa_aiocb_t matrix_a_aiocb, matrix_b_aiocb;

// This function sends a submatrix request on IO Cluster for both matrix a and b
static void
_ask_submatrix(int ya, int yb, int rank)
{
	DMSG(" ##### asking submatrices a[%d] and b[%d]\n", ya, yb);
	matrix_getsub_t getsub;
	getsub.rank = rank;
	getsub.ya = ya;
	getsub.yb = yb;
	if (mppa_write(queue_getsub_fd, &getsub, sizeof(matrix_getsub_t)) < 0) {
		EMSG("mppa_write queue_getsub failed\n");
		mppa_exit(1);
	}
}

// This function swap curA and curB to the next pipeline stage
// It implies that user have finished to compute with curA and curB
// This function waits that previous requested transfer is finished, then it swap curA/transA and curB/transB (except
// for first request)
static void
swap_submatrices(int step __attribute__ ((unused)) )
{
	static int current_buffer = 0;

	tuto_db_t (*_locA)[cluster_m][global_n] = (tuto_db_t(*)[cluster_m][global_n])locA;
	tuto_db_t (*_locB)[cluster_m][global_n] = (tuto_db_t(*)[cluster_m][global_n])locB;

	// Wait for the end of previous transfer (except for first request)
	static int first_request = 1;
	if (!first_request) {
		// This particualr tracepoint shows the efficiency of double buffering: these tracepoints must be as short as
		// possible.
		// In this specific example (c = a.x + b), computation is very light, then this tracepoint will show a poor
		// efficiency of double buffering.
		mppa_tracepoint(tuto_db, wait_previous_transfer_end__in, step);
		mppa_aio_wait(&matrix_a_aiocb);
		mppa_aio_wait(&matrix_b_aiocb);
		mppa_tracepoint(tuto_db, wait_previous_transfer_end__out);
	} else
		first_request = 0;

	// Swap transferring and computing submatrices
	curA = (tuto_db_t*)_locA[current_buffer];
	curB = (tuto_db_t*)_locB[current_buffer];
	current_buffer = (current_buffer + 1) % PIPELINE_DEPTH;
	transA = (tuto_db_t*)_locA[current_buffer];
	transB = (tuto_db_t*)_locB[current_buffer];
}

// This function swap compute/transfer buffers, and request next needed submatrices
// y is the offset (in lines) on the distant buffers a and b
// submatrix size is already known (cluster_matrix_size)
static void
get_next_submatrix_and_swap(int y, int step)
{

	mppa_tracepoint(tuto_db, get_next_submatrix_and_swap__in, step);
	swap_submatrices(step);

	// Prepare reception for transfering a and b submatrices
	mppa_aiocb_ctor(&matrix_a_aiocb, matrix_a_in_fd, transA, cluster_matrix_size);
	mppa_aio_read(&matrix_a_aiocb);
	mppa_aiocb_ctor(&matrix_b_aiocb, matrix_b_in_fd, transB, cluster_matrix_size);
	mppa_aio_read(&matrix_b_aiocb);

	mppa_tracepoint(tuto_db, get_next_submatrix_and_swap__out);

	_ask_submatrix(y, y, mppa_getpid());
}

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

int
main(__attribute__((unused)) int argc, char *argv[])
{

	if (argc != 12) {
		EMSG("Incorrect arguments\n");
		mppa_exit(1);
	}

	mppa_tracepoint(tuto_db, init_nocipc__in);

	DMSG("Start cluster %d\n", mppa_getpid());

	int argn = 1;

	const char *matrix_a_in_path = argv[argn++];
	const char *matrix_b_in_path = argv[argn++];
	const char *matrix_c_out_path = argv[argn++];
	const char *getsub_queue_path = argv[argn++];
	const char *sync_io_to_clusters_path = argv[argn++];
	const char *sync_clusters_to_io_path = argv[argn++];

	global_n_str = argv[argn++];
	global_m_str = argv[argn++];
	cluster_m_str = argv[argn++];
	nb_clusters_str = argv[argn++];
	nb_threads_str = argv[argn++];

	global_n = convert_str_to_ul(global_n_str);
	global_m = convert_str_to_ul(global_m_str);
	cluster_m = convert_str_to_ul(cluster_m_str);
	nb_clusters = convert_str_to_ul(nb_clusters_str);
	nb_threads = convert_str_to_ul(nb_threads_str);

	global_matrix_size = global_n * global_m * sizeof(tuto_db_t);
	cluster_matrix_size = global_n * cluster_m * sizeof(tuto_db_t);

	unsigned long nb_submatrices_per_cluster = global_m / cluster_m / nb_clusters;

	locA = memalign(64, PIPELINE_DEPTH * cluster_matrix_size);
	locB = memalign(64, PIPELINE_DEPTH * cluster_matrix_size);
	locC = memalign(64, cluster_matrix_size);

	if (!locA || !locB || !locC) {
		EMSG("Memory allocation failed: locA: %p, locB: %p, locC, %p\n", locA, locB, locC);
		mppa_exit(1);
	}
	curA = locA;
	curB = locB;
	int sync_io_to_cluster_fd = -1;
	int sync_clusters_to_io_fd = -1;
	if (mppa_init_barrier(sync_io_to_clusters_path, sync_clusters_to_io_path, &sync_io_to_cluster_fd,
		&sync_clusters_to_io_fd)) {
		EMSG("mppa_init_barrier failed\n");
		mppa_exit(1);
	}

	DMSG("Open portal %s\n", argv[1]);
	if ((matrix_a_in_fd = mppa_open(matrix_a_in_path, O_RDONLY)) < 0) {
		EMSG("Can't open portal\n");
		mppa_exit(1);
	}
	DMSG("Open portal %s\n", argv[2]);
	if ((matrix_b_in_fd = mppa_open(matrix_b_in_path, O_RDONLY)) < 0) {
		EMSG("Can't open portal\n");
		mppa_exit(1);
	}
	DMSG("Open portal %s\n", argv[3]);
	if ((matrix_c_out_fd = mppa_open(matrix_c_out_path, O_WRONLY)) < 0) {
		EMSG("Can't write result");
		mppa_exit(1);
	}
	// We don't want to send notifications to IO Cluster for result submatrices transfers, as these notifications
	// disturb the IO Cluster execution.
	// We will only send one final notification, to ensure that all data has well been received by IO Cluster.
	if (mppa_ioctl(matrix_c_out_fd, MPPA_TX_NOTIFY_OFF) != 0) {
		EMSG("Can't MPPA_TX_NOTIFY_OFF on matrix_c_out_fd\n");
		mppa_exit(1);
	}
	DMSG("Open queue_getsub %s\n", argv[4]);
	if ((queue_getsub_fd = mppa_open(getsub_queue_path, O_WRONLY)) < 0) {
		EMSG("Can't open queue getsub\n");
		mppa_exit(1);
	}


	if (mppa_barrier(sync_io_to_cluster_fd, sync_clusters_to_io_fd)) {
		EMSG("mppa_barrier failed\n");
		mppa_exit(1);
	}


	int step = 0;
	unsigned long first_y = nb_submatrices_per_cluster * cluster_m * mppa_getpid();
	mppa_aiocb_t aiocb_write_c;
	mppa_aiocb_ctor(&aiocb_write_c, matrix_c_out_fd, locC, cluster_matrix_size);
	// Initiate first request before enter in the big loop
	get_next_submatrix_and_swap(first_y, step);
	// Big job

	for (int cur_y = first_y, step = 0;
		step < nb_submatrices_per_cluster;
		cur_y += cluster_m, step++) {

		if (step < nb_submatrices_per_cluster - 1)
			get_next_submatrix_and_swap(cur_y + cluster_m, step);
		else
			swap_submatrices(step);

		// We wait the last possible time to wait on end of transfer of matrix C,
		// before overwriting it
		if (step != 0) {
			mppa_tracepoint(tuto_db, wait_end_C_transfer__in, step);
			mppa_aio_wait(&aiocb_write_c);
			mppa_tracepoint(tuto_db, wait_end_C_transfer__out);
		}

		print_matrix("submatrix a", cluster_m, global_n, curA);
		print_matrix("submatrix b", cluster_m, global_n, curB);
		mppa_tracepoint(tuto_db, kernel__in, step);
		compute_matrices(cluster_m, global_n, (tuto_db_t(*)[cluster_m])curA, (tuto_db_t(*)[cluster_m])curB,
			(tuto_db_t(*)[cluster_m])locC, TUTO_K, nb_threads);
		mppa_tracepoint(tuto_db, kernel__out);
		print_matrix("submatrix c", cluster_m, global_n, locC);

 #ifdef __k1__
		__k1_wmb();
 #endif
		DMSG("  ***** send C to offset %llu\n", (unsigned long long)(cur_y * global_n) * sizeof(tuto_db_t));
		mppa_aiocb_set_pwrite (&aiocb_write_c,
			locC,
			cluster_matrix_size,
			(cur_y * global_n) * sizeof(tuto_db_t));
		mppa_aio_write(&aiocb_write_c);
	}

	mppa_tracepoint(tuto_db, wait_end_C_transfer__in, step);
	mppa_aio_wait(&aiocb_write_c);
	mppa_tracepoint(tuto_db, wait_end_C_transfer__out);

	// Send one final notification to IO Cluster for result submatrices, to ensure that all data has been well received.
	if (mppa_ioctl(matrix_c_out_fd, MPPA_TX_NOTIFY_ON) != 0) {
		EMSG("Can't MPPA_TX_NOTIFY_ON on matrix_c_out_fd\n");
		mppa_exit(1);
	}
	mppa_pwrite(matrix_c_out_fd, NULL, 0, 0);

	mppa_tracepoint(tuto_db, main_loop__out);

	mppa_close_barrier(sync_io_to_cluster_fd, sync_clusters_to_io_fd);
	mppa_exit(0);

}
