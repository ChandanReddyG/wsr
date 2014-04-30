#ifndef __WSR_TASK__FUNCTIONS_H__
#define __WSR_TASK__FUNCTIONS_H__

#include "wsr_util.h"

 typedef int (*WSR_TASK_FUNC)(int);

 WSR_TASK_FUNC wsr_get_function_ptr(int task_type);

#endif
