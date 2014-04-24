#include <stdlib.h>
#include <malloc.h>

#include <mppaipc.h>
#include <mppa/osconfig.h>

#include "wsr_util.h"
#include "wsr_task.h"
#include "wsr_cdeque.h"

// Global Variables

//ID of compute cluster
static unsigned long cluster_id;

//Total numeber of clusters
static unsigned long nb_cluster;

//Number of threads to use
static unsigned long nb_threads;

//global channel IDs used for communication
static cosnt char *channel_io_to_cc_0;
static cosnt char *channel_io_to_cc_1;

static cosnt char *channel_cc_to_io_0;
static cosnt char *channel_cc_to_io_1;

static int channel_cc_to_io_0_fd, channel_io_to_cc_0_fd;
static int channel_cc_to_io_1_fd, channel_io_to_cc_1_fd;

int main(int argc, char *argv[])
{

    cluster_id = mppa_getpid();

    DMSG("Started proc on cluster %d\n", cluster_id);

    int argn = 1;
    //const char *sync_io_to_cc = argv[argn++];
    //const char *sync_cc_to_io = argv[argn++];

    channel_io_to_cc_0 = argv[argn++];
    channel_io_to_cc_1 = argv[argn++];

    channel_cc_to_io_0 = argv[argn++];
    channel_cc_to_io_1 = argv[argn++];

    DMSG("Open portal %s\n", channel_io_to_cc_0);
    if(channel_io_to_cc_0_fd = mppa_open(channel_io_to_cc_0, O_READONLY) < 0) {
        EMSG("Open portal failed for %s\n", channel_io_to_cc_0);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_io_to_cc_1);
    if(channel_io_to_cc_1_fd = mppa_open(channel_io_to_cc_1, O_READONLY) < 0) {
        EMSG("Open portal failed for %s\n", channel_io_to_cc_1);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_cc_to_io_0);
    if(channel_cc_to_io_0_fd = mppa_open(channel_cc_to_io_0, O_WRONLY) < 0) {
        EMSG("Open portal failed for %s\n", channel_cc_to_io_0);
        mppa_exit(1);
    }

    DMSG("Open portal %s\n", channel_cc_to_io_1);
    if(channel_cc_to_io_1_fd = mppa_open(channel_cc_to_io_1, O_WRONLY) < 0) {
        EMSG("Open portal failed for %s\n", channel_cc_to_io_1);
        mppa_exit(1);
    }


    mppa_close(channel_io_to_cc_0);
    mppa_close(channel_io_to_cc_1);

    mppa_close(channel_cc_to_io_0);
    mppa_close(channel_cc_to_io_1);

    mppa_exit(0);

}


