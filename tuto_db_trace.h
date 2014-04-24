/*
 * Copyright (C) 2013 Kalray SA. All rights reserved.
 * This code is Kalray proprietary and confidential.
 * Any use of the code for whatever purpose is subject to
 * specific written permission of Kalray SA.
 */

#undef MPPA_TRACEPOINT_PROVIDER
#define MPPA_TRACEPOINT_PROVIDER tuto_db
#undef MPPA_TRACEPOINT_FILE
#define MPPA_TRACEPOINT_FILE matrix_trace.h

#if !defined(_MPPA_MM_TRACE_H_) || defined(MPPA_TRACEPOINT_HEADER_MULTI_READ)
#  define _MPPA_MM_TRACE_H_

#  include <mppa_trace.h>

MPPA_DECLARE_TRACEPOINT(tuto_db, main__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, main__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, init_pcie__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, init_pcie__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, recv_pcie__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, recv_pcie__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, init_nocipc__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, init_nocipc__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, spawn__in,
	( MPPA_TRACEPOINT_DEC_FIELD(short, nb_clusters) )
)
MPPA_DECLARE_TRACEPOINT(tuto_db, spawn__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, synchro__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, synchro__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, wait_last_result__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, wait_last_result__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, send_results_to_host__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, send_results_to_host__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, wait_previous_transfer_end__in,
	( MPPA_TRACEPOINT_DEC_FIELD(short, step) )
)
MPPA_DECLARE_TRACEPOINT(tuto_db, wait_previous_transfer_end__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, wait_ucore_resource__in,
	( MPPA_TRACEPOINT_DEC_FIELD(short, rank) )
)
MPPA_DECLARE_TRACEPOINT(tuto_db, wait_ucore_resource__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, get_next_submatrix_and_swap__in,
	( MPPA_TRACEPOINT_DEC_FIELD(short, rank) )
)
MPPA_DECLARE_TRACEPOINT(tuto_db, get_next_submatrix_and_swap__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, wait_end_C_transfer__in,
	( MPPA_TRACEPOINT_DEC_FIELD(short, step) )
)
MPPA_DECLARE_TRACEPOINT(tuto_db, wait_end_C_transfer__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, wait_last_results__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, wait_last_results__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, kernel__in,
	( MPPA_TRACEPOINT_DEC_FIELD(short, step) )
)
MPPA_DECLARE_TRACEPOINT(tuto_db, kernel__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, submat_server__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, submat_server__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, main_loop__in, () )
MPPA_DECLARE_TRACEPOINT(tuto_db, main_loop__out, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, one_cluster_finished, () )

MPPA_DECLARE_TRACEPOINT(tuto_db, info,
	(
		MPPA_TRACEPOINT_STRING_FIELD(char*, str),
		MPPA_TRACEPOINT_DEC_FIELD(int, value)
	)
)

#endif
