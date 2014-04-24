/*
 * Copyright (C) 2013 Kalray SA. All rights reserved.
 * This code is Kalray proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of Kalray SA.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <mppaipc.h>

// BSP values can't be read from host in multibinary.
#define BSP_NB_DMA_IO 4

#include "tuto_db.h"

#define DEF "\x1B[0m"
#define RED "\x1B[31m"

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

// MPPAIPC PCIe buffer used to transfer matrix a from host to IO
static const char a_buffer_path[] = "/mppa/buffer/128#1";
// MPPAIPC PCIe buffer used to transfer matrix b from host to IO
static const char b_buffer_path[] = "/mppa/buffer/128#2";
// MPPAIPC PCIe buffer used to transfer result matrix from IO to host
static const char r_buffer_path[] = "/mppa/buffer/host#3";

// Random matrix population
static void
fill_matrix(tuto_db_t (*mat)[global_n])
{
	for (int i = 0; i < global_m; i++) {
		for (int j = 0; j < global_n; j++) {
			// Check if tuto_db_t is int or double/float
			if (__builtin_types_compatible_p(int, tuto_db_t) != 0) {
				mat[i][j] = rand() % 100;
			} else {
				mat[i][j] = (tuto_db_t)rand() / (RAND_MAX + 1.0);
			}
		}
	}
}

// Compute a and b matrices on MPPA
static int
compute_on_mppa(tuto_db_t (*a)[global_n], tuto_db_t (*b)[global_n], tuto_db_t (*c)[global_n])
{
	int ret;
	int status = 0;
	int mppa_status;
	int mppa_pid;

	//////////////////////////////
	// Load multibinary on MPPA //
	//////////////////////////////

	DMSG("Loading %s\n", MULTI_BIN_NAME);
	status = mppa_load(MULTI_BIN_NAME, 0);
	if (status < 0) {
		EMSG("mppa_load failed\n");
		return -1;
	}

	/////////////////////////////////////
	// MPPAIPC PCIe connectors opening //
	/////////////////////////////////////

	int a_buffer_fd = mppa_open(a_buffer_path, O_WRONLY);
	if (a_buffer_fd < 0) {
		EMSG("Failed to open a_buffer_fd\n");
		return -1;
	}
	int b_buffer_fd = mppa_open(b_buffer_path, O_WRONLY);
	if (b_buffer_fd < 0) {
		EMSG("Failed to open b_buffer_fd\n");
		return -1;
	}
	int r_buffer_fd = mppa_open(r_buffer_path, O_RDONLY);
	if (r_buffer_fd < 0) {
		EMSG("Failed to open r_buffer_fd\n");
		return -1;
	}

	///////////////////////////////////
	// Spawn IO binary on IO Cluster //
	///////////////////////////////////

	const char *argv[] =
	{ IO_BIN_NAME, global_n_str, global_m_str, cluster_m_str, nb_clusters_str, nb_threads_str,
		  a_buffer_path, b_buffer_path, r_buffer_path, 0 };
	const void *partition[] = { "/mppa/nodes/128", 0 };
	mppa_pid = mppa_spawn(-1, partition, IO_BIN_NAME, argv, 0);
	if (mppa_pid < 0) {
		EMSG("Failed to spawn\n");
		return -1;
	}

	//////////////////////////////////////////////////////////////
	// Transfer of matrices a and b from host to IO Cluster DDR //
	//////////////////////////////////////////////////////////////

	DMSG("Transfering a and b matrices (%.2f kB)\n", (float)(global_matrix_size * 2) / 1024);
	ret = mppa_pwrite(a_buffer_fd, a, global_matrix_size, 0);
	if (ret != global_matrix_size) {
		EMSG("Failed to write %llu bytes from fd a\n", global_matrix_size);
		return -1;
	}
	ret = mppa_pwrite(b_buffer_fd, b, global_matrix_size, 0);
	if (ret != global_matrix_size) {
		EMSG("Failed to write %llu bytes from fd b\n", global_matrix_size);
		return -1;
	}

	///////////////////////////////////////////////////
	// Wait to receive result matrix from IO Cluster //
	///////////////////////////////////////////////////

	DMSG("Waiting for result matrix\n");
	ret = mppa_read(r_buffer_fd, c, global_matrix_size);
	if (ret != global_matrix_size) {
		EMSG("Failed to read %llu bytes from fd r\n", global_matrix_size);
		return -1;
	}
	DMSG("Result matrix received\n");

	//////////////////////////////////////
	// Wait IO Cluster end of execution //
	//////////////////////////////////////

	status = mppa_waitpid(mppa_pid, &mppa_status, 0);
	if (status < 0) {
		EMSG("mppa_waitpid failed\n");
		return -1;
	}

	if (mppa_status != 0) {
		EMSG("IO Cluster failed\n");
		return -1;
	}

	status = mppa_unload(MULTI_BIN_NAME);
	if (status < 0) {
		EMSG("mppa_unload failed\n");
		return -1;
	}
	DMSG("IO Cluster succeded\n");

	return 0;
}

int
main(int argc, char **argv)
{
	int res;

	if (argc != 6) {
		EMSG("Usage: %s global_n global_m cluster_m nb_clusters nb_threads\n", argv[0]);
		return 1;
	}

	int argn = 1;
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

	if ((global_n | global_m | cluster_m | nb_clusters | nb_threads) < 0) {
		EMSG(
			"global_n=%s global_m=%s cluster_m=%s nb_clusters=%s nb_threads=%s must be positive integer values\n",
			global_n_str,
			global_m_str,
			cluster_m_str,
			nb_clusters_str,
			nb_threads_str);
		return 1;
	}

	if (global_m % (cluster_m * nb_clusters) != 0) {
		EMSG("global_m=%lu must be a multiple of ( cluster_m x nb_clusters ) = %lu\n",
			global_m,
			cluster_m * nb_clusters);
		return 1;
	}

	if (nb_clusters % BSP_NB_DMA_IO != 0) {
		EMSG("nb_clusters=%lu must be a multiple of the number of IO DMA interfaces=%d\n",
			nb_clusters,
			BSP_NB_DMA_IO);
	}

	global_matrix_size = global_n * global_m * sizeof(tuto_db_t);
	cluster_matrix_size = global_n * cluster_m * sizeof(tuto_db_t);

	printf("Tutorial double buffering:\n"
	       "Global matrix %lu x %lu\n"
	       "Local matrices  %lu x %lu\n"
	       "%lu clusters\n"
	       "%lu threads / cluster\n"
	       "DDR memory needed: %llu kB\n"
	       "SMEM memory needed (per cluster): %llu kB\n"
		, global_n, global_m, cluster_m, global_n
		, nb_clusters, nb_threads,
		(global_matrix_size * 3) / 1024,
		(cluster_matrix_size * 5) / 1024
		);


	tuto_db_t (*a)[global_n], (*b)[global_n], (*c_host)[global_n], (*c_mppa)[global_n];

	a = malloc(global_matrix_size);
	b = malloc(global_matrix_size);
	c_host = malloc(global_matrix_size);
	c_mppa = malloc(global_matrix_size);

	if (!a || !b || !c_host || !c_mppa) {
		EMSG("Can't malloc one of these buffers: a=%p , b=%p, c_host=%p, c_mppa=%p\n", a, b, c_host, c_mppa);
		exit(1);
	}

	fill_matrix(a);
	fill_matrix(b);

	if ((res = compute_on_mppa(a, b, c_mppa)) != 0)
		return res;

	DMSG("Compute matrix multiply on host\n");
	compute_matrices(global_n, global_m, a, b, c_host, TUTO_K, 4);

	int diff_print = lines;

	if (memcmp(c_mppa, c_host, global_matrix_size) != 0) {
		EMSG("Bad result!!!!\n");

		if (diff_print == global) {
			printf("Diff:\n");
			for (int i = 0; i < global_m; i++) {
				printf("\n");
				for (int j = 0; j < global_n; j++) {
					if (c_host[i][j] != c_mppa[i][j]) {
						printf("X");
					} else {
						printf("•");
					}
				}
			}
			printf("\n");
			printf("Get: \n");
			for (int i = 0; i < global_m; i++) {
				for (int j = 0; j < global_n; j++) {
					if (c_host[i][j] != c_mppa[i][j])
						printf(" %6"TUTO_FORMAT, c_mppa[i][j]);
				}
				printf("\n");
			}

			printf("Expect: \n");
			for (int i = 0; i < global_m; i++) {
				for (int j = 0; j < global_n; j++) {
					if (c_host[i][j] != c_mppa[i][j])
						printf(" %6"TUTO_FORMAT, c_host[i][j]);
				}
				printf("\n");
			}
		} else if (diff_print == lines) {
			for (int i = 0; i < global_m; i++) {
				if (memcmp(c_mppa[i], c_host[i], global_n * sizeof(tuto_db_t)) != 0) {
					printf("Diff at line %d:\n", i);
					for (int j = 0; j < global_n; j++) {
						if (c_host[i][j] != c_mppa[i][j]) {
							printf(RED "X"DEF);
						} else {
							printf("•");
						}
					}
					printf("\n");
					printf("Get:\n");
					for (int j = 0; j < global_n; j++) {
						if (c_host[i][j] != c_mppa[i][j])
							printf(RED);
						printf(" %6"TUTO_FORMAT, c_mppa[i][j]);
						if (c_host[i][j] != c_mppa[i][j])
							printf(DEF);
					}
					printf("\n");
					printf("Expect:\n");
					for (int j = 0; j < global_n; j++) {
						if (c_host[i][j] != c_mppa[i][j])
							printf(RED);
						printf(" %6"TUTO_FORMAT, c_host[i][j]);
						if (c_host[i][j] != c_mppa[i][j])
							printf(DEF);
					}
					printf("\n");
				}
			}
		}
		return -1;
	}
	printf("Results OK\n");
	return 0;
}
