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
#define BLOCK_SIZE 5
#define GLOBAL_MATRIX_SIZE 80


#define A_ID 0
#define B_ID 1
#define C_ID 2

void init_matrix();

WSR_TASK_LIST_P get_matmul_task_list(int cluster_id, int num_clusters, int cur_iteration);
void copy_back_output(WSR_TASK_LIST_P task_list, int cluster_id, int num_clusters, int cur_iteration);
int block_matrix_multiply_task(WSR_BUFFER_LIST_P buffer_list, int block_size);
int verify_matmul_result();

#endif /* MATMUL_TASKS_H_ */
