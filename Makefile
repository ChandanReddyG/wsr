# Needed for Ubuntu/Debian since sh points to dash instead of bash...
SHELL := /bin/bash

# Where board can be developer/explorer
board := developer

# enable traces (on or off)
traces := on

# Where target can be in simulator/hardware/native
platforms := hardware simulator

# Define weither we are using OSes or not. can be os/rtems/nodeos/bare
system-name := os

# Global cflags and lflags
cflags := -Wall -I. -std=gnu99 -DMPPA_TRACE_ENABLE

k1-cflags := -g3 -Wall -O3

cluster-cflags := ${k1-cflags} -fopenmp
cluster-lflags := ${k1-lflags} -fopenmp -lgomp

io-cflags := ${k1-cflags} -Wno-float-equal
io-lflags := ${k1-lflags}

mppa-bin := wsr_multibin

# The multibinary mymultibin will be made of
wsr_multibin-objs := wsr_io wsr_cc
wsr_multibin-boot := wsr_io
wsr_multibin-flags := -w ".*"

# Cluster binaries
cluster-bin := wsr_cc

# Each *-srcs variable describe sources for binary
wsr_cc-srcs := wsr_compute_thread.c wsr_buffer.c wsr_seralize.c wsr_task_functions.c wsr_task.c 
#wsr_cc-srcs :=  wsr_buffer.c wsr_task.c 
# The io-bin var is used to build groups of var
io-bin := wsr_io

wsr_io-srcs := wsr_io_thread.c wsr_buffer.c wsr_seralize.c wsr_task_functions.c wsr_task.c wsr_util.c
# Flags can be specified by sources
wsr_io-cflags := -DCLUSTER_BIN_NAME=\"wsr_cc\"

host-cflags := -DMULTI_BIN_NAME=\"wsr_multibin.mpk\" -DIO_BIN_NAME=\"wsr_io\" -I${K1_TOOLCHAIN_DIR}/include -fopenmp
host-lflags := -L${K1_TOOLCHAIN_DIR}/lib64 -fopenmp -lgomp
host-bin := wsr_host

wsr_host-srcs := tuto_db_host.c common.c compute.c

# Finally, include the Kalray Makefile which will instantiate the rules
# and do the work
include ${K1_TOOLCHAIN_DIR}/share/make/Makefile.mppaipc

run_hw: wsr_host_hw wsr_multibin
	cd $(BIN_DIR); ./wsr_host_hw 1024 8192 64 1 1

run_hw_trace: wsr_host_hw wsr_multibin
	cd $(BIN_DIR); ${K1_TOOLCHAIN_DIR}/bin/k1-trace-util -a -l 0XFF --background=./k1-trace-util.pid ; ./wsr_host_hw 1024 8192 64 16 16 ; kill -SIGINT `cat ./k1-trace-util.pid`
	${K1_TOOLCHAIN_DIR}/bin/k1-stv -hwtrace $(BUILD_DIR)/wsr_io:$(BIN_DIR)/trace.dump.4 -hwtrace $(BUILD_DIR)/wsr_cc:$(BIN_DIR)/trace.dump.0 -hwtrace $(BUILD_DIR)/wsr_cc:$(BIN_DIR)/trace.dump.1 -hwtrace $(BUILD_DIR)/wsr_cc:$(BIN_DIR)/trace.dump.2 -hwtrace $(BUILD_DIR)/wsr_cc:$(BIN_DIR)/trace.dump.3 &

run_sim: wsr_host_sim wsr_multibin
	cd $(BIN_DIR); $(K1_TOOLCHAIN_DIR)/bin/k1-pciesim-runner ./wsr_host_sim 512 1024 64 16 16
