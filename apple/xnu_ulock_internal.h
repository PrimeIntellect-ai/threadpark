/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#ifndef _SYS_ULOCK_H
#define _SYS_ULOCK_H

#include <mach/mach_port.h>
#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value,
    uint32_t timeout);             /* timeout is specified in microseconds */
extern int __ulock_wait2(uint32_t operation, void *addr, uint64_t value,
    uint64_t timeout, uint64_t value2);
extern int __ulock_wake(uint32_t operation, void *addr, uint64_t wake_value);


/*
 * operation bits [7, 0] contain the operation code.
 *
 * NOTE: make sure to add logic for handling any new
 *       types to kdp_ulock_find_owner()
 */
#define UL_COMPARE_AND_WAIT             1
#define UL_UNFAIR_LOCK                  2
#define UL_COMPARE_AND_WAIT_SHARED      3
#define UL_UNFAIR_LOCK64_SHARED         4
#define UL_COMPARE_AND_WAIT64           5
#define UL_COMPARE_AND_WAIT64_SHARED    6
/* obsolete names */
#define UL_OSSPINLOCK                   UL_COMPARE_AND_WAIT
#define UL_HANDOFFLOCK                  UL_UNFAIR_LOCK
/* These operation code are only implemented in (DEVELOPMENT || DEBUG) kernels */
#define UL_DEBUG_SIMULATE_COPYIN_FAULT  253
#define UL_DEBUG_HASH_DUMP_ALL          254
#define UL_DEBUG_HASH_DUMP_PID          255

/*
 * operation bits [15, 8] contain the flags for __ulock_wake
 */
#define ULF_WAKE_ALL                    0x00000100
#define ULF_WAKE_THREAD                 0x00000200
#define ULF_WAKE_ALLOW_NON_OWNER        0x00000400

/*
 * operation bits [23, 16] contain the flags for __ulock_wait
 *
 * @const ULF_WAIT_WORKQ_DATA_CONTENTION
 * The waiter is contending on this lock for synchronization around global data.
 * This causes the workqueue subsystem to not create new threads to offset for
 * waiters on this lock.
 *
 * @const ULF_WAIT_CANCEL_POINT
 * This wait is a cancelation point
 *
 * @const ULF_WAIT_ADAPTIVE_SPIN
 * Use adaptive spinning when the thread that currently holds the unfair lock
 * is on core.
 */
#define ULF_WAIT_WORKQ_DATA_CONTENTION  0x00010000
#define ULF_WAIT_CANCEL_POINT           0x00020000
#define ULF_WAIT_ADAPTIVE_SPIN          0x00040000

/*
 * operation bits [31, 24] contain the generic flags
 */
#define ULF_NO_ERRNO                    0x01000000

/*
 * masks
 */
#define UL_OPCODE_MASK          0x000000FF
#define UL_FLAGS_MASK           0xFFFFFF00
#define ULF_GENERIC_MASK        0xFFFF0000

#define ULF_WAIT_MASK           (ULF_NO_ERRNO | \
	                         ULF_WAIT_WORKQ_DATA_CONTENTION | \
	                         ULF_WAIT_CANCEL_POINT | ULF_WAIT_ADAPTIVE_SPIN)

#define ULF_WAKE_MASK           (ULF_NO_ERRNO | \
	                         ULF_WAKE_ALL | \
	                         ULF_WAKE_THREAD | \
	                         ULF_WAKE_ALLOW_NON_OWNER)

#endif /* PRIVATE */

__END_DECLS