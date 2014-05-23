
#ifndef __WS_UTIL_H__
#define __WS_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
 #include <string.h>
#include <assert.h>
 #include <pthread.h>

//size of buffers used in double buffering scheme: 0.1 MB
#define BUFFER_SIZE 170000

//#define BUFFER_SIZE 192
#define PIPELINE_DEPTH 3

#define EXIT_TASK_ID -3

unsigned long
convert_str_to_ul(const char *str);

#  define IS_DEBUG 0

#  if IS_DEBUG == 1
#    define DMSG(fmt, ...)                                    \
	do {                                    \
		printf("<%3d> " fmt, mppa_getpid(),  ## __VA_ARGS__);   \
	} while (0)
#  else
#    define DMSG(fmt, ...) do { } while (0)
#  endif

# define EMSG(fmt, ...)                                                    \
	do {                                                                    \
		fprintf(stderr, "<%3d> ERROR: " fmt,  mppa_getpid(), ## __VA_ARGS__);   \
	} while (0)

#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })

#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })

int get_prev_state(int state);
int get_next_state(int state);

#endif
