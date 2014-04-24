
#ifndef __WS_UTIL_H__
#define __WS_UTIL_H__

#define MAX_THREADS_PER_CLUSTER 16
#define NUM_CLUSTERS 16

unsigned long
convert_str_to_ul(const char *str);

#  define IS_DEBUG 0

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

#endif
