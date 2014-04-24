
#include <stdlib.h>
#include <malloc.h>

#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "wsr_util.h"
#include "wsr_task.h"

// Global Variables

//ID of compute cluster
static unsigned long cluster_id;

//Total numeber of clisters
static unsigned long nb_cluster;

//Number of threads to use
static unsigned long nb_threads;



