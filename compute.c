/*
 * Copyright (C) 2013 Kalray SA. All rights reserved.
 * This code is Kalray proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of Kalray SA.
 */

#include "tuto_db.h"

void
compute_matrices(int matrix_h,
	int matrix_w,
	tuto_db_t (*a)[matrix_w],
	tuto_db_t (*b)[matrix_w],
	tuto_db_t (*c)[matrix_w],
	tuto_db_t k,
	int _nb_threads)
{
	#pragma omp parallel num_threads(_nb_threads)
	{
 #if __GNUC__ > 4 || (__GNUC__ == 4 && (__GNUC_MINOR__ > 4 || (__GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ > 2)))
		#  pragma omp for schedule(static) collapse(2)
 #else
		#  pragma omp for schedule(static)
 #endif
		for (int i = 0; i < matrix_h; i++) {
			for (int j = 0; j < matrix_w; j++) {
				c[i][j] = a[i][j] * k + b[i][j];
			}
		}
	}
}
