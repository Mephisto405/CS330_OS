#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stddef.h>
#include <list.h>
#include "threads/palloc.h"
#include "threads/thread.h"

struct frame{
	void* kpage;                    /* Address of kernel virtual page */
	                                /* palloc_get_page returns kpage as kernel address. */
	struct page* page;              /* Supplemental page */
	struct thread* holder;          /* Who holds this structure? */
	struct list_elem elem;          /* List element */
};

void frame_init(void);
void framelock_acquire(void);
void framelock_release(void);
void* frame_get_without_pagedir_set(struct page* page, enum palloc_flags flags);
void frame_free_without_pagedir_clear(void* kpage);

#endif /* vm/frame.h */
