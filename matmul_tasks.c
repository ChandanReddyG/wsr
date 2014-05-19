/*
 * matmul_tasks.c
 *
 *      Author: accesscore
 */


#include <stdio.h>
#include "matmul_tasks.h"
#include "wsr_task.h"

static double *a, *b, *c, *d;

void
block_matrix_multiply(int block_size,
			double *a, 
			double *b, 
			double *c) 
{
  int i, j, k;

  for (i = 0; i < block_size; ++i)
    for (j = 0; j < block_size; ++j)
      for (k = 0; k < block_size; ++k)
		  c[i*block_size + j] += a[i*block_size + k] * b[k*block_size + j];


		  return;
}



int block_matrix_multiply_task(WSR_BUFFER_LIST_P buffer_list, int block_size){

	DMSG("block matrix  multiply task is called\n");
	int i = 0;

	WSR_BUFFER_P c_buf = buffer_list->buf_ptr;
	assert(c_buf != NULL);
	double *c = (double *)c_buf->buf;
	DMSG("C buff ptr = %lu\n", c);
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

    DMSG("matrix a \n");
	print_matrix(block_size, a);
    DMSG("matrix b \n");
	print_matrix(block_size, b);
    DMSG("matrix c \n");
	print_matrix(block_size, c);

	block_matrix_multiply(block_size, a, b, c);

    DMSG("matrix c after task execution\n");
	print_matrix(block_size, c);
    
	return 0;
}



void copy_block_matrix(double *a, double *block_a, int a1, int a2){

	int i, j;
    int row = 0, col = 0;

	for(i=0;i<BLOCK_SIZE;i++){
        row = a1 * BLOCK_SIZE + i;
		for(j=0;j<BLOCK_SIZE;j++){
            col = a2 * BLOCK_SIZE + j;
			block_a[i*BLOCK_SIZE + j] = a[row * GLOBAL_MATRIX_SIZE + col];
		}
	}

	return;
}

void copy_back_block_matrix(double *a, char  *block_a, int a1, int a2){

	DMSG("Copying back c1 = %d  c2 = %d block_a = %lu \n", a1, a2, block_a);
	int i, j;
	double *a_temp;
    int row = 0, col = 0;

	for(i=0;i<BLOCK_SIZE;i++){
        row = a1 * BLOCK_SIZE + i;
		for(j=0;j<BLOCK_SIZE;j++){
            col = a2 * BLOCK_SIZE + j;
			a_temp = 	&(a[row * GLOBAL_MATRIX_SIZE + col]);
			memcpy(a_temp, block_a, sizeof(double));
            DMSG("C[%d][%d] = %f \n", row, col, a[row*GLOBAL_MATRIX_SIZE + col]);
			block_a += sizeof(double);
		}
	}

	return;
}

WSR_TASK_P create_block_matmul_task(int a1, int a2, int b1, int b2, int c1, int c2, WSR_BUFFER_P c_buf){


	int task_id = a1 * BLOCK_SIZE + a2 + b1 * BLOCK_SIZE + b2 + c1*BLOCK_SIZE + c2;
	WSR_TASK_P task = wsr_task_alloc(MATMUL_TASK_ID, task_id, 0);

    int num_buffers_per_row = GLOBAL_MATRIX_SIZE/BLOCK_SIZE;
    int num_buffers_per_column = GLOBAL_MATRIX_SIZE/BLOCK_SIZE;
    int total_buffers = num_buffers_per_row * num_buffers_per_column;

	DMSG("creating block matmul task c1 = %d c2 = %d, a1 = %d, a2 = %d, b1 = %d, b2 = %d id = %d\n",
			c1, c2, a1, a2, b1, b2, task_id);

	int block_matrix_size = BLOCK_SIZE * BLOCK_SIZE * sizeof(double);
	DMSG("block_matrix_size = %d\n", block_matrix_size);
    int buf_id;

	if(c_buf == NULL){
		double *block_c = malloc(block_matrix_size);
		copy_block_matrix(c, block_c, c1, c2);
        buf_id = C_ID * total_buffers + c1 * num_buffers_per_row + c2; 
        DMSG("Buf id of C buffer with c1 = %d c2 = %d, id = %d\n", c1, c2, buf_id);
		c_buf = wsr_buffer_create(block_matrix_size, buf_id, block_c);
	}
		DMSG("C_bug for %d task = %lu\n", task_id, c_buf->id);
	wsr_task_add_dependent_buffer(task, c_buf);

	double *block_a = malloc(block_matrix_size);
	copy_block_matrix(a, block_a, a1, a2);
    buf_id = A_ID * total_buffers + a1 * num_buffers_per_row + a2; 
    DMSG("Buf id of A buffer with a1 = %d a2 = %d, id = %d\n", a1, a2, buf_id);
    print_matrix(BLOCK_SIZE, block_a);
	WSR_BUFFER_P a_buf = wsr_buffer_create(block_matrix_size, buf_id, block_a);
	wsr_task_add_dependent_buffer(task, a_buf);


	double *block_b = malloc(block_matrix_size);
	copy_block_matrix(b, block_b, b1, b2);
    buf_id = B_ID * total_buffers + b1 * num_buffers_per_row + b2; 
    DMSG("Buf id of B buffer with b1 = %d b2 = %d, id = %d\n", b1, b2, buf_id);
    print_matrix(BLOCK_SIZE, block_b);
	WSR_BUFFER_P b_buf = wsr_buffer_create(block_matrix_size, buf_id, block_b);
	wsr_task_add_dependent_buffer(task, b_buf);

	task->param = BLOCK_SIZE;

    DMSG("task %d dep buffes = %d\n",task->id, task->num_buffers);

	return task;
}

void init_matrix(){
	int i, j;

	a = (double *)malloc(GLOBAL_MATRIX_SIZE * GLOBAL_MATRIX_SIZE * sizeof(double));
	for(i=0;i<GLOBAL_MATRIX_SIZE;i++)
		for(j=0;j<GLOBAL_MATRIX_SIZE;j++)
			a[i*GLOBAL_MATRIX_SIZE + j] = 2.0; //rand();


	b = (double *)malloc(GLOBAL_MATRIX_SIZE * GLOBAL_MATRIX_SIZE * sizeof(double));
	for(i=0;i<GLOBAL_MATRIX_SIZE;i++)
		for(j=0;j<GLOBAL_MATRIX_SIZE;j++)
			b[i*GLOBAL_MATRIX_SIZE + j] = 3;// rand();

	c = (double *)malloc(GLOBAL_MATRIX_SIZE * GLOBAL_MATRIX_SIZE * sizeof(double));
	for(i=0;i<GLOBAL_MATRIX_SIZE;i++)
		for(j=0;j<GLOBAL_MATRIX_SIZE;j++)
			c[i*GLOBAL_MATRIX_SIZE + j] = 0;

	d = (double *)malloc(GLOBAL_MATRIX_SIZE * GLOBAL_MATRIX_SIZE * sizeof(double));
	for(i=0;i<GLOBAL_MATRIX_SIZE;i++)
		for(j=0;j<GLOBAL_MATRIX_SIZE;j++)
			d[i*GLOBAL_MATRIX_SIZE + j] = 0;
}

WSR_TASK_LIST_P  get_block_matmul_task_list_(int c1, int c2, int num_blocks){

	WSR_TASK_LIST_P task_list = wsr_task_list_create(NULL);

	int a1, a2, b1, b2;

    int num_buffers_per_row = GLOBAL_MATRIX_SIZE/BLOCK_SIZE;
    int num_buffers_per_column = GLOBAL_MATRIX_SIZE/BLOCK_SIZE;
    int total_buffers = num_buffers_per_row * num_buffers_per_column;

	int block_matrix_size = BLOCK_SIZE * BLOCK_SIZE * sizeof(double);
	double *block_c = malloc(block_matrix_size);
	DMSG("C-ptr = %lu\n", block_c);
	copy_block_matrix(c, block_c, c1, c2);
    int buf_id = C_ID * total_buffers + c1 * num_buffers_per_row + c2; 
    DMSG("Buf id of C buffer with c1 = %d c2 = %d, id = %d\n", c1, c2, buf_id);
	WSR_BUFFER_P c_buf = wsr_buffer_create(block_matrix_size, buf_id, block_c);

	WSR_TASK_P prev_task = NULL, cur_task;
	a1 = c1; b2 = c2;
	a2 = 0; b1 = 0;
	for(int i = 0; i<num_blocks; i++){
              cur_task = create_block_matmul_task(a1, a2, b1, b2, c1, c2, c_buf);
              wsr_task_list_add(task_list, cur_task);

              if(prev_task != NULL)
            	  wsr_task_add_dependent_task(prev_task, cur_task);

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

    print_matrix(GLOBAL_MATRIX_SIZE, c);
	for(int i = 0;  i < num_blocks; i++){


		WSR_BUFFER_P c_buf =  task_list->task->buffer_list->buf_ptr;
		assert(c_buf != NULL);
		DMSG("task_id = %d c buf size = %d, id = %d\n\n", task_list->task->id, c_buf->size, c_buf->id);
        DMSG("Copying back C1 = %d, c2 = %d\n", c1, i);
		copy_back_block_matrix(c, c_buf->buf, c1, i);
		DMSG("Copy done\n");
        print_matrix(GLOBAL_MATRIX_SIZE, c);
		DMSG("\n\n");

		for(int j = 0; j<num_blocks -1;j++){
			task_list = task_list->next;
			assert(task_list != NULL);
		}

	}

	return;

}

#define EPLISON 1E-6
int compare_matrices( int size,
            double *a, 
            double *b){

	double diff = 0.0;
	for(int i = 0; i< size;i++){
		for(int j = 0; j < size; j++){
			diff = fabs(a[i*size + j] - b[i*size + j]);

			if(diff > EPLISON )
				return 0;
		}
	}

	return 1;
}

void print_matrix(int size, double *a){

	int i, j;
	for(i = 0; i<size; i++){
		for(j=0;j<size;j++)
			printf("%f\t", a[i*size + j]);
		printf("\n");
	}
}

int verify_matmul_result(){

	print_matrix(GLOBAL_MATRIX_SIZE, a);
	DMSG("C = \n");
	print_matrix(GLOBAL_MATRIX_SIZE, c);

	block_matrix_multiply(GLOBAL_MATRIX_SIZE, a, b, d);

	DMSG("D = \n");
	print_matrix(GLOBAL_MATRIX_SIZE, d);

	return compare_matrices(GLOBAL_MATRIX_SIZE, c,d);
}
