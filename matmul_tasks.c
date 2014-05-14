/*
 * matmul_tasks.c
 *
 *      Author: accesscore
 */


#include <stdio.h>
#include "matmul_tasks.h"
#include "wsr_task.h"

static double *a, *b, *c;

static inline void
block_matrix_multiply(int block_size,
            double (*block_a)[block_size][block_size],
            double (*block_b)[block_size][block_size],
            double (*block_c)[block_size][block_size])
{
  int i, j, k;

  for (i = 0; i < block_size; ++i)
    for (j = 0; j < block_size; ++j)
      for (k = 0; k < block_size; ++k)
    (*block_c)[i][j] += (*block_a)[i][k] * (*block_b)[k][j];
}



int block_matrix_multiply_task(WSR_BUFFER_LIST_P buffer_list, int block_size){

	DMSG("block matrix  multiply task is called\n");
	int i = 0;

	WSR_BUFFER_P c_buf = buffer_list->buf_ptr;
	assert(c_buf != NULL);
	double *c = (double *)c_buf->buf;
	int num_elem_c = c_buf->size / sizeof(double);

	buffer_list = buffer_list->next;
	WSR_BUFFER_P a_buf = buffer_list->buf_ptr;
	assert(a_buf != NULL);
	double *a = (double *)a_buf->buf;
	int num_elem_a = a_buf->size / sizeof(double);

	buffer_list = buffer_list->next;
	WSR_BUFFER_P b_buf = buffer_list->buf_ptr;
	assert(b_buf != NULL);
	double *b = (double *)b_buf->buf;
	int num_elem_b = b_buf->size / sizeof(double);



	//DMSG("a = %d, b = %d, c =%d\n", num_elem_a, num_elem_b, num_elem_c);
	assert((num_elem_a == num_elem_b) && (num_elem_a== num_elem_c));


	block_matrix_multiply(block_size, a, b, c);

	return 0;
}



void copy_block_matrix(double *a, double *block_a, int a1, int a2){

	int i, j;

	for(i=0;i<BLOCK_SIZE;i++){
		for(j=0;j<BLOCK_SIZE;j++){
			block_a[i*BLOCK_SIZE + j] = a[(a1 + i)*BLOCK_SIZE + (a2 + j)];
		}
	}

	return;
}

void copy_back_block_matrix(double *a, double *block_a, int a1, int a2){

	DMSG("Copying back c1 = %d  c2 = %d block_a = %lu \n", a1, a2, block_a);
	int i, j;

	for(i=0;i<BLOCK_SIZE;i++){
		for(j=0;j<BLOCK_SIZE;j++){
//		 a[(a1 + i)*BLOCK_SIZE + (a2 + j)] += 1;
			DMSG(" i = %d, j = %d\n", i, j);
		block_a[i*BLOCK_SIZE + j] += 1;
//		 a[(a1 + i)*BLOCK_SIZE + (a2 + j)]= block_a[i*BLOCK_SIZE + j];
		}
	}

	return;
}

WSR_TASK_P create_block_matmul_task(int a1, int a2, int b1, int b2, int c1, int c2, WSR_BUFFER_P c_buf){


	int task_id = a1 * BLOCK_SIZE + a2 + b1 * BLOCK_SIZE + b2 + c1*BLOCK_SIZE + c2;
	WSR_TASK_P task = wsr_task_alloc(MATMUL_TASK_ID, task_id, 0);

	DMSG("creating block matmul task c1 = %d c2 = %d, a1 = %d, a2 = %d, b1 = %d, b2 = %d id = %d\n",
			c1, c2, a1, a2, b1, b2, task_id);

	int block_matrix_size = BLOCK_SIZE * BLOCK_SIZE * sizeof(double);
	DMSG("block_matrix_size = %d\n", block_matrix_size);

	if(c_buf == NULL){
		double *block_c = malloc(block_matrix_size);
		copy_block_matrix(c, block_c, c1, c2);
		c_buf = wsr_buffer_create(block_matrix_size, 1, block_c);
	}
	wsr_task_add_dependent_buffer(task, c_buf);

	double *block_a = malloc(block_matrix_size);
	copy_block_matrix(a, block_a, a1, a2);
	WSR_BUFFER_P a_buf = wsr_buffer_create(block_matrix_size, 0, block_a);
	wsr_task_add_dependent_buffer(task, a_buf);


	double *block_b = malloc(block_matrix_size);
	copy_block_matrix(b, block_b, b1, b2);
	WSR_BUFFER_P b_buf = wsr_buffer_create(block_matrix_size, 0, block_b);
	wsr_task_add_dependent_buffer(task, b_buf);



	task->param = BLOCK_SIZE;

	return task;
}

void init_matrix(){
	int i, j;

	a = (double *)malloc(GLOBAL_MATRIX_SIZE * GLOBAL_MATRIX_SIZE * sizeof(double));
	for(i=0;i<GLOBAL_MATRIX_SIZE;i++)
		for(j=0;j<GLOBAL_MATRIX_SIZE;j++)
			a[i*GLOBAL_MATRIX_SIZE + j] = rand();

	b = (double *)malloc(GLOBAL_MATRIX_SIZE * GLOBAL_MATRIX_SIZE * sizeof(double));
	for(i=0;i<GLOBAL_MATRIX_SIZE;i++)
		for(j=0;j<GLOBAL_MATRIX_SIZE;j++)
			b[i*GLOBAL_MATRIX_SIZE + j] = rand();

	c = (double *)malloc(GLOBAL_MATRIX_SIZE * GLOBAL_MATRIX_SIZE * sizeof(double));
	for(i=0;i<GLOBAL_MATRIX_SIZE;i++)
		for(j=0;j<GLOBAL_MATRIX_SIZE;j++)
			c[i*GLOBAL_MATRIX_SIZE + j] = 0;

}

WSR_TASK_LIST_P  get_block_matmul_task_list_(int c1, int c2, int num_blocks){

	WSR_TASK_LIST_P task_list = wsr_task_list_create(NULL);

	int a1, a2, b1, b2;


	int block_matrix_size = BLOCK_SIZE * BLOCK_SIZE * sizeof(double);
	double *block_c = malloc(block_matrix_size);
	copy_block_matrix(c, block_c, c1, c2);
	WSR_BUFFER_P c_buf = wsr_buffer_create(block_matrix_size, 1, block_c);

	WSR_TASK_P prev_task = NULL, cur_task;
	a1 = c1; b2 = c2;
	a2 = 0; b1 = 0;
	for(int i = 0; i<num_blocks; i++){
              cur_task = create_block_matmul_task(a1, a2, b1, b2, c1, c2, c_buf);
              wsr_task_list_add(task_list, cur_task);

//              if(prev_task != NULL)
//            	  wsr_task_add_dependent_task(prev_task, cur_task);

              a2++;
              b1++;

              prev_task = cur_task;

	}

	return task_list;

}

WSR_TASK_LIST_P get_matmul_task_list(int cluster_id, int num_clusters, int cur_iteration){

	WSR_TASK_LIST_P task_list = NULL;

	int num_blocks = GLOBAL_MATRIX_SIZE / BLOCK_SIZE;
	int chunk_size = num_blocks / num_clusters;

	int  c1 = cluster_id * chunk_size + cur_iteration;

	WSR_TASK_LIST_P cur_task_list = NULL;
	for(int i = 0;  i < num_blocks; i++){

		cur_task_list = get_block_matmul_task_list_(c1, i, num_blocks);
		task_list = wsr_task_list_append(task_list, cur_task_list);

	}
	return task_list;
}

void copy_back_output(WSR_TASK_LIST_P task_list, int cluster_id, int num_clusters, int cur_iteration){

	DMSG("Copy back function\n");

	int num_blocks = GLOBAL_MATRIX_SIZE / BLOCK_SIZE;
	int chunk_size = num_blocks / num_clusters;

	int  c1 = cluster_id * chunk_size + cur_iteration;

	for(int i = 0;  i < num_blocks; i++){


		WSR_BUFFER_P c_buf =  task_list->task->buffer_list->buf_ptr;
		assert(c_buf != NULL);
		DMSG("task_id = %d c buf size = %d, id = %d\n\n", task_list->task->id, c_buf->size, c_buf->id);
		copy_back_block_matrix(c, c_buf->buf, c1, i);
		DMSG("Copy done\n");

		for(int j = 0; j<num_blocks -1;j++){
			task_list = task_list->next;
			assert(task_list != NULL);
		}

	}

	return;

}
