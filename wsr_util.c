
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "wsr_util.h"

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



