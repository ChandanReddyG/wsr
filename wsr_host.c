/*
 * wsr_host.c
 *
 *      Author: accesscore
 */



#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <mppaipc.h>

// BSP values can't be read from host in multibinary.
#define BSP_NB_DMA_IO 4

//#include "tuto_db.h"
#include "wsr_util.h"

// Number of clusters
static unsigned long nb_clusters;
// Number of threads per cluster
static unsigned long nb_threads;


static const char *nb_threads_str;
static const char *nb_clusters_str;


int
main(int argc, char **argv)
{
	int argn = 1;

	nb_clusters_str = argv[argn++];
	nb_threads_str = argv[argn++];

	nb_clusters = convert_str_to_ul(nb_clusters_str);
	nb_threads = convert_str_to_ul(nb_threads_str);

	if ((nb_clusters | nb_threads) < 0) {
		EMSG(
			" nb_clusters=%s nb_threads=%s must be positive integer values\n",
			nb_clusters_str,
			nb_threads_str);
		return 1;
	}

	int status = 0;
	int mppa_status;
	int mppa_pid;

	DMSG("Loading %s\n", MULTI_BIN_NAME);
	status = mppa_load(MULTI_BIN_NAME, 0);
	if (status < 0) {
		EMSG("mppa_load failed\n");
		return -1;
	}

	const char *argv_io[] =
	{ IO_BIN_NAME, nb_clusters_str, nb_threads_str , 0};
	const void *partition[] = { "/mppa/nodes/128", 0 };
	mppa_pid = mppa_spawn(-1, partition, IO_BIN_NAME, argv_io, 0);
	if (mppa_pid < 0) {
		EMSG("Failed to spawn\n");
		return -1;
	}

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



