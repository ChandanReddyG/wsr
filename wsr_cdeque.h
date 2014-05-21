/* Implementation of the work-stealing algorithm for load-balancing
   from: CHASE, D. AND LEV, Y. 2005. Dynamic circular work-stealing
   deque. In Proceedings of the seventeenth annual ACM symposium on
   Parallelism in algorithms and architectures. SPAA'05.

   http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.170.1097&rep=rep1&type=pdf
   */

#ifndef __WSR_CDEQUE_H__
#define __WSR_CDEQUE_H__

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#include "stdatomic.h"
#include "wsr_task.h"
#include "atomic-defs-c11.h"
#include "cbuffer-c11.h"

typedef struct cdeque cdeque_t, *cdeque_p;

struct cdeque
{
  atomic_size_t bottom __attribute__ ((aligned (64)));
  atomic_size_t top __attribute__ ((aligned (64)));
  atomic_size_t  num_tasks  __attribute__((aligned(64)));
  cbuffer_atomic_p cbuffer __attribute__ ((aligned (64)));
};

static inline void
cdeque_init (cdeque_p cdeque, size_t log_size)
{
  atomic_init (&cdeque->bottom, 0);
  atomic_init (&cdeque->top, 0);
  atomic_init (&cdeque->cbuffer, cbuffer_alloc (log_size));
}

/* Alloc and initialize the deque with log size LOG_SIZE.  */
static inline cdeque_p
cdeque_alloc (size_t log_size)
{
  cdeque_p cdeque = (cdeque_p) malloc (sizeof (cdeque_t));
  if (cdeque == NULL)
    EMSG("Out of memory ...");
  cdeque_init (cdeque, log_size);
  return cdeque;
}

/* Dealloc the CDEQUE.  */
static inline void
cdeque_free (cdeque_p cdeque)
{
  cbuffer_free (atomic_load_explicit (&cdeque->cbuffer, relaxed));
  free (cdeque);
}

/* Print the elements in the deque CDEQUE.  */
static inline void
print_cdeque (cdeque_p cdeque)
{
  size_t i;
  size_t bottom = atomic_load_explicit (&cdeque->bottom, relaxed);
  size_t top = atomic_load_explicit (&cdeque->top, acquire);
  cbuffer_p buffer = atomic_load_explicit (&cdeque->cbuffer, relaxed);
  for (i = top; i < bottom; i++)
    printf ("%p,", cbuffer_get (buffer, i, relaxed));
  printf ("\n");
}

 void cdeque_push_bottom (cdeque_p, WSR_TASK_P, int);
 void cdeque_push_task(int , WSR_TASK_P );
 int cdeque_try_push_bottom (cdeque_p, WSR_TASK_P);
 WSR_TASK_P cdeque_take (cdeque_p, int);
 WSR_TASK_P cdeque_steal (cdeque_p);
  void *wsr_cdeque_execute(void *arg);
 void wsr_init_cdeques(int num_threads);
 void wsr_add_to_cdeque(WSR_TASK_LIST_P task_list, int num_tasks, int num_threads, int thread_id);
void wsr_add_to_single_cdeque(WSR_TASK_LIST_P task_list, int thread_id);

#endif
