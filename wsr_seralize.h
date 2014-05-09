/*
 * wsr_seralize.h
 *
 *  Created on: Apr 30, 2014
 *      Author: accesscore
 */

#ifndef WSR_SERALIZE_H_
#define WSR_SERALIZE_H_

#include "wsr_task.h"

int  wsr_serialize_tasks(WSR_TASK_LIST *task_list, char *buf);
WSR_TASK_LIST_P wsr_deseralize_tasks(char *buf, int *size, int *num_tasks);


#endif /* WSR_SERALIZE_H_ */
