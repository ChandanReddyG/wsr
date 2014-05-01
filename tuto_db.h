/*
 * Copyright (C) 2013 Kalray SA. All rights reserved.
 * This code is Kalray proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of Kalray SA.
 */

#ifndef __TUTO_DB_H__
#  define __TUTO_DB_H__

#  define PIPELINE_DEPTH 2

typedef int tuto_db_t;
#  define TUTO_FORMAT "d"
#  define TUTO_K 2

enum {
	global,
	lines,
};

typedef struct {
	int	rank;
	int	ya;
	int	yb;
} matrix_getsub_t;

unsigned long
convert_str_to_ul(const char *str);

void
compute_matrices(int matrix_h,
	int matrix_w,
	tuto_db_t (*a)[matrix_w],
	tuto_db_t (*b)[matrix_w],
	tuto_db_t (*c)[matrix_w],
	tuto_db_t k,
	int _nb_threads);


#  define IS_DEBUG 1

#  if IS_DEBUG == 1
#    define DMSG(fmt, ...)                                    \
	do {                                                    \
		printf("<%3d> " fmt, mppa_getpid(), ## __VA_ARGS__);   \
	} while (0)
#  else
#    define DMSG(fmt, ...) do { } while (0)
#  endif

#  define EMSG(fmt, ...)                                                    \
	do {                                                                    \
		fprintf(stderr, "<%3d> ERROR: " fmt, mppa_getpid(), ## __VA_ARGS__);   \
	} while (0)
#endif

// #define PRINT_MATRIX
void
_print_matrix(const char *msg, int matrix_h, int matrix_w, tuto_db_t *_mat);
#ifdef PRINT_MATRIX
#  define print_matrix(m, h, w, p) _print_matrix(m, h, w, p)
#else
#  define print_matrix(m, h, w, p)
#endif
