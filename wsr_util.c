
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



int get_prev_state(int state){

	switch(state){
	case 0:
		return 2;
	case 1:
		return 0;
	case 2:
		return 1;
	default:
		assert(0);
		return -1;
	}
}


int get_next_state(int state){

	switch(state){
	case 0:
		return 1;
	case 1:
		return 2;
	case 2:
		return 0;
	default:
		assert(0);
		return -1;
	}
}
