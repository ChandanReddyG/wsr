/*
 * wsr_trace.h
 *
 *      Author: accesscore
 */

#ifndef WSR_TRACE_H_
#define WSR_TRACE_H_

#undef MPPA_TRACEPOINT_PROVIDER
#define MPPA_TRACEPOINT_PROVIDER wsr
#undef MPPA_TRACEPOINT_FILE
#define MPPA_TRACEPOINT_FILE wsr_trace.h


#if !defined(_MPPA_MM_TRACE_H_) || defined(MPPA_TRACEPOINT_HEADER_MULTI_READ)
#  define _MPPA_MM_TRACE_H_
#  include <mppa_trace.h>

MPPA_DECLARE_TRACEPOINT(wsr, main__in, () )
MPPA_DECLARE_TRACEPOINT(wsr, main__out, () )

MPPA_DECLARE_TRACEPOINT(wsr, cc_main__in, (MPPA_TRACEPOINT_DEC_FIELD(int, thread_id)) )
MPPA_DECLARE_TRACEPOINT(wsr, cc_main__out, (MPPA_TRACEPOINT_DEC_FIELD(int, thread_id)) )

MPPA_DECLARE_TRACEPOINT(wsr, portal_open__in, () )
MPPA_DECLARE_TRACEPOINT(wsr, portal_open__out, () )

MPPA_DECLARE_TRACEPOINT(wsr, cc_portal_open__in, (MPPA_TRACEPOINT_DEC_FIELD(int, thread_id)) )
MPPA_DECLARE_TRACEPOINT(wsr, cc_portal_open__out, (MPPA_TRACEPOINT_DEC_FIELD(int, thread_id)) )

MPPA_DECLARE_TRACEPOINT(wsr, sync__in, () )
MPPA_DECLARE_TRACEPOINT(wsr, sync__out, () )

MPPA_DECLARE_TRACEPOINT(wsr, cc_sync__in, (MPPA_TRACEPOINT_DEC_FIELD(int, thread_id)) )
MPPA_DECLARE_TRACEPOINT(wsr, cc_sync__out, (MPPA_TRACEPOINT_DEC_FIELD(int, thread_id)) )

MPPA_DECLARE_TRACEPOINT(wsr, starting_thread, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id) ))

MPPA_DECLARE_TRACEPOINT(wsr, thread__in, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id) ))
MPPA_DECLARE_TRACEPOINT(wsr, thread__out, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id) ))


MPPA_DECLARE_TRACEPOINT(wsr, start_aync_read_executed_tasks, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

MPPA_DECLARE_TRACEPOINT(wsr, start_aync_write_of_ready_tasks, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

MPPA_DECLARE_TRACEPOINT(wsr, io_wait_till_ready_task_transfer_completion__in,  ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

MPPA_DECLARE_TRACEPOINT(wsr,io_wait_till_ready_task_transfer_completion__out,  ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

 MPPA_DECLARE_TRACEPOINT(wsr, io_wait_till_executed_task_transfer_completion__in,  ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

 MPPA_DECLARE_TRACEPOINT(wsr, io_wait_till_executed_task_transfer_completion__out,  ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

		 MPPA_DECLARE_TRACEPOINT(wsr, seralize__in, () )
		 MPPA_DECLARE_TRACEPOINT(wsr, seralize__out, () )

 MPPA_DECLARE_TRACEPOINT(wsr, start_sync_read_of_read_tasks, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

MPPA_DECLARE_TRACEPOINT(wsr, wait_till_ready_task_transfer_completion__in, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		MPPA_TRACEPOINT_DEC_FIELD(int, state) ))
 MPPA_DECLARE_TRACEPOINT(wsr, wait_till_ready_task_transfer_completion__out, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

		 MPPA_DECLARE_TRACEPOINT(wsr, start_async_write_of_executed_tasks, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
				 MPPA_TRACEPOINT_DEC_FIELD(int, state) ))


 MPPA_DECLARE_TRACEPOINT(wsr, wait_till_executed_task_transfer_completion__in, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
		MPPA_TRACEPOINT_DEC_FIELD(int, state) ))
  MPPA_DECLARE_TRACEPOINT(wsr, wait_till_executed_task_transfer_completion__out, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
          MPPA_TRACEPOINT_DEC_FIELD(int, state) ))

 		 MPPA_DECLARE_TRACEPOINT(wsr, deseralize__in, () )
 		 MPPA_DECLARE_TRACEPOINT(wsr, deseralize__out, () )

  MPPA_DECLARE_TRACEPOINT(wsr, task_execute__in, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
          MPPA_TRACEPOINT_DEC_FIELD(int, task_id) ))
  MPPA_DECLARE_TRACEPOINT(wsr, task_execute__out, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
          MPPA_TRACEPOINT_DEC_FIELD(int, task_id) ))

          MPPA_DECLARE_TRACEPOINT(wsr, try_steal__in, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
                  MPPA_TRACEPOINT_DEC_FIELD(int, victim) ))
          MPPA_DECLARE_TRACEPOINT(wsr, try_steal__out, ( MPPA_TRACEPOINT_DEC_FIELD(int, thread_id),
                  MPPA_TRACEPOINT_DEC_FIELD(int, victim) ))

		 MPPA_DECLARE_TRACEPOINT(wsr, atomic_store__in, () )
		 MPPA_DECLARE_TRACEPOINT(wsr, atomic_store__out, () )

		 MPPA_DECLARE_TRACEPOINT(wsr, atomic_load__in, () )
		 MPPA_DECLARE_TRACEPOINT(wsr, atomic_load__out, () )

		 MPPA_DECLARE_TRACEPOINT(wsr, atomic_CAS__in, () )
		 MPPA_DECLARE_TRACEPOINT(wsr, atomic_CAS__out, () )

#endif

#endif /* WSR_TRACE_H_ */
