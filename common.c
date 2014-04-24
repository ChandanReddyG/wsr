/*
 * Copyright (C) 2013 Kalray SA. All rights reserved.
 * This code is Kalray proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of Kalray SA.
 */

#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "tuto_db.h"

unsigned long
convert_str_to_ul(const char *str)
{
	char *endptr;
	errno = 0; /* To distinguish success/failure after call */
	unsigned long val = strtoul(str, &endptr, 10);

	/* Check for various possible errors */
	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
		|| (errno != 0 && val == 0)) {
		return -1;
	}
	if (endptr == str) {
		return -1;
	}
	return val;
}

void
_print_matrix(const char *msg, int matrix_h, int matrix_w, tuto_db_t *_mat)
{
	printf("%s\n", msg);
	tuto_db_t (*mat)[matrix_w] = (tuto_db_t (*)[matrix_w])_mat;
	for (int i = 0; i < matrix_h; i++) {
		for (int j = 0; j < matrix_w; j++) {
			printf("%"TUTO_FORMAT " ", mat[i][j]);
		}
		printf("\n");
	}
}
