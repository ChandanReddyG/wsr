/*
 * matmul_tasks.h
 *
 *  Created on: May 12, 2014
 *      Author: accesscore
 */

#ifndef MATMUL_TASKS_H_
#define MATMUL_TASKS_H_

#include "wsr_task.h"

#define MATMUL_TASK_ID 5
#define BLOCK_SIZE 8
#define GLOBAL_MATRIX_SIZE 128

#define NUM_GROUPED_TASKS 16

#define A_ID 0
#define B_ID 1
#define C_ID 2

void init_matrix();

WSR_TASK_LIST_P get_matmul_task_list(int cluster_id, int num_clusters,
		int cur_iteration, int col_start, int col_end, int chunk_start, int chunk_end);
void copy_back_output(WSR_TASK_LIST_P task_list, int cluster_id, int num_clusters, int cur_iteration,
		int col_start, int col_end, int next_task_dist);
int block_matrix_multiply_task(WSR_BUFFER_LIST_P buffer_list, int block_size);
int verify_matmul_result();

#endif /* MATMUL_TASKS_H_ */
