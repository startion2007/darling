/*
 * Copyright (c) 1999-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import <malloc/malloc.h>
#import <mach/vm_statistics.h>

#define stack_logging_type_free		0
#define stack_logging_type_generic	1	/* anything that is not allocation/deallocation */
#define stack_logging_type_alloc	2	/* malloc, realloc, etc... */
#define stack_logging_type_dealloc	4	/* free, realloc, etc... */
#define stack_logging_type_vm_allocate  16      /* vm_allocate or mmap */
#define stack_logging_type_vm_deallocate  32	/* vm_deallocate or munmap */
#define stack_logging_type_mapped_file_or_shared_mem	128

// The valid flags include those from VM_FLAGS_ALIAS_MASK, which give the user_tag of allocated VM regions.
#define stack_logging_valid_type_flags ( \
	stack_logging_type_generic | \
	stack_logging_type_alloc | \
	stack_logging_type_dealloc | \
	stack_logging_type_vm_allocate | \
	stack_logging_type_vm_deallocate | \
	stack_logging_type_mapped_file_or_shared_mem | \
	VM_FLAGS_ALIAS_MASK);

// Following flags are absorbed by stack_logging_log_stack()
#define	stack_logging_flag_zone		8	/* NSZoneMalloc, etc... */
#define stack_logging_flag_cleared	64	/* for NewEmptyHandle */

#define STACK_LOGGING_VM_USER_TAG(flags) (((flags) & VM_FLAGS_ALIAS_MASK) >> 24)


/* Macro used to disguise addresses so that leak finding can work */
#define STACK_LOGGING_DISGUISE(address)	((address) ^ 0x00005555) /* nicely idempotent */

extern int stack_logging_enable_logging; /* when clear, no logging takes place */
extern int stack_logging_dontcompact; /* default is to compact; when set does not compact alloc/free logs; useful for tracing history */
extern int stack_logging_finished_init; /* set after we've returned from the Libsystem initialiser */
extern int stack_logging_postponed; /* set if we needed to postpone logging till after initialisation */


extern void stack_logging_log_stack(unsigned type, unsigned arg1, unsigned arg2, unsigned arg3, unsigned result, unsigned num_hot_to_skip);
	/* This is the old log-to-memory logger, which is now deprecated.  It remains for compatibility with performance tools that haven't been updated to disk_stack_logging_log_stack() yet. */

extern void __disk_stack_logging_log_stack(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t size, uintptr_t ptr_arg, uintptr_t return_val, uint32_t num_hot_to_skip);
	/* Fits as the malloc_logger; logs malloc/free/realloc events and can log custom events if called directly */


/* 64-bit-aware stack log access.  As new SPI, these routines are prefixed with double-underscore to avoid conflict with Libsystem clients. */

typedef struct {
	uint32_t		type_flags;
	uint64_t		stack_identifier;
	uint64_t		argument;
	mach_vm_address_t	address;
} mach_stack_logging_record_t;

extern kern_return_t __mach_stack_logging_set_file_path(task_t task, char* file_path);

extern kern_return_t __mach_stack_logging_get_frames(task_t task, mach_vm_address_t address, mach_vm_address_t *stack_frames_buffer, uint32_t max_stack_frames, uint32_t *count);
    /* Gets the last allocation record (malloc, realloc, or free) about address */

extern kern_return_t __mach_stack_logging_enumerate_records(task_t task, mach_vm_address_t address, void enumerator(mach_stack_logging_record_t, void *), void *context);
    /* Applies enumerator to all records involving address sending context as enumerator's second parameter; if !address, applies enumerator to all records */

extern kern_return_t __mach_stack_logging_frames_for_uniqued_stack(task_t task, uint64_t stack_identifier, mach_vm_address_t *stack_frames_buffer, uint32_t max_stack_frames, uint32_t *count);
    /* Given a uniqued_stack fills stack_frames_buffer */


#pragma mark -
#pragma mark Legacy

/* The following is the old 32-bit-only, in-process-memory stack logging.  This is deprecated and clients should move to the above 64-bit-aware disk stack logging SPI. */

typedef struct {
    unsigned	type;
    unsigned	uniqued_stack;
    unsigned	argument;
    unsigned	address; /* disguised, to avoid confusing leaks */
} stack_logging_record_t;

typedef struct {
    unsigned	overall_num_bytes;
    unsigned	num_records;
    unsigned	lock; /* 0 means OK to lock; used for inter-process locking */
    unsigned	*uniquing_table; /* allocated using vm_allocate() */
            /* hashtable organized as (PC, uniqued parent)
            Only the second half of the table is active
            To enable us to grow dynamically */
    unsigned	uniquing_table_num_pages; /* number of pages of the table */
    unsigned	extra_retain_count; /* not used by stack_logging_log_stack */
    unsigned	filler[2]; /* align to cache lines for better performance */
    stack_logging_record_t	records[0]; /* records follow here */
} stack_logging_record_list_t;

extern stack_logging_record_list_t *stack_logging_the_record_list;
    /* This is the global variable containing all logs */

extern kern_return_t stack_logging_get_frames(task_t task, memory_reader_t reader, vm_address_t address, vm_address_t *stack_frames_buffer, unsigned max_stack_frames, unsigned *num_frames);
    /* Gets the last record in stack_logging_the_record_list about address */

#define STACK_LOGGING_ENUMERATION_PROVIDED	1	// temporary to avoid dependencies between projects

extern kern_return_t stack_logging_enumerate_records(task_t task, memory_reader_t reader, vm_address_t address, void enumerator(stack_logging_record_t, void *), void *context);
    /* Gets all the records about address;
    If !address, gets all records */

extern kern_return_t stack_logging_frames_for_uniqued_stack(task_t task, memory_reader_t reader, unsigned uniqued_stack, vm_address_t *stack_frames_buffer, unsigned max_stack_frames, unsigned *num_frames);
    /* Given a uniqued_stack fills stack_frames_buffer */



extern void thread_stack_pcs(vm_address_t *buffer, unsigned max, unsigned *num);
    /* Convenience to fill buffer with the PCs of the frames, starting with the hot frames;
    num: returned number of frames
    */

