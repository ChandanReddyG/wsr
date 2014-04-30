/*
 * wsr_seralize.h
 *
 *  Created on: Apr 30, 2014
 *      Author: accesscore
 */

#ifndef WSR_SERALIZE_H_
#define WSR_SERALIZE_H_

#include "wsr_task.h"

char* wsr_seralize_tasks(WSR_TASK_LIST *task_list, int *size);
WSR_TASK_LIST_P wsr_deseralize_tasks(char *buf, int size);


#endif /* WSR_SERALIZE_H_ */
