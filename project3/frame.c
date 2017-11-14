#include "vm/frame.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/page.h"
#include <stdbool.h>
#include <stdint.h>
#include <debug.h>

static struct list frame_list;
static struct lock frame_lock;			              /* lock for frame_list */
static void* frame_evict_once(enum palloc_flags flags);		/* Eviction */
static struct frame* select_frame_to_evict(void);	/* Select a frame to be evicted */

/*
 * We must make below functions as WRAPPER functions for palloc.c
 * palloc_get_page = frame_get
 * palloc_free_page = frame_free
 * etc
 */

void
frame_init(void)
{
	list_init(&frame_list);
	lock_init(&frame_lock);
}

void
framelock_acquire(void)
{
	lock_acquire(&frame_lock);
}

void
framelock_release(void)
{
	lock_release(&frame_lock);
}

void* 
frame_get_without_pagedir_set(struct page* page, enum palloc_flags flags)
{
	void* kpage;
	struct frame* frame;
	kpage = palloc_get_page(PAL_USER | flags);

	while( kpage == NULL ){
		
		kpage = frame_evict_once(flags);
		framelock_release();
	}
	
	if( kpage != NULL ){
		frame = (struct frame *)malloc(sizeof(struct frame));
		frame->kpage = kpage;
		frame->page = page;
		frame->holder = thread_current();
		
		framelock_acquire();
		list_push_back(&frame_list, &frame->elem);
		framelock_release();
		
		return kpage;
	}
	else{
		return NULL;
	}

}

void 
frame_free_without_pagedir_clear(void* kpage)
{
	struct list_elem* e;
	struct frame* frame;

	framelock_acquire();
	for( e = list_begin (&frame_list); e != list_end(&frame_list);
		 e = list_next(e) ){
		frame = list_entry(e, struct frame, elem);
		if( frame->kpage == kpage ){
			list_remove(e);
			//free(frame);
			palloc_free_page(kpage);	// free the page in user pool
      free(frame);
			break;
		}
	}
	framelock_release();
}

/* Second chance algorithm */
static void*
frame_evict_once(enum palloc_flags flags)
{
  struct list_elem* e;
  struct frame* frame;
  struct page* curr_page;
  struct thread* holder;

  framelock_acquire();
  /* Select a frame to be evicted */
  e = list_begin(&frame_list);
  while(true){
    frame = list_entry(e, struct frame, elem);
    curr_page = frame->page;
    /* Evict only an un-pinned frame. Pinned frames must be protected like lock. */
    if( curr_page->pinned == false ){
      holder = frame->holder;
      if( pagedir_is_accessed(holder->pagedir, curr_page->upage) == false ){
        /* Select */
        goto selected;
      }
      else{
        /* Give second chance to a frame */
        pagedir_set_accessed(holder->pagedir, curr_page->upage, false);
      }

    }
    /* Circular search */
    e = list_next(e);
    if( e == list_end(&frame_list) ){
      e = list_begin(&frame_list);
    }
  }
 
selected:
  /* Swap */
  if( pagedir_is_dirty(holder->pagedir, curr_page->upage) ){
      //curr_page->type == SWAP ){ // is this condition needed?
     curr_page->type = SWAP;
     curr_page->swap_index = swap_out(frame->kpage);
     curr_page->swapped = true;
  }
  /* Deallocate the supplemental page info */
  list_remove(&frame->elem);
  curr_page->loaded = false;
  pagedir_clear_page(holder->pagedir, curr_page->upage);
  palloc_free_page(frame->kpage);
  free(frame);
  return palloc_get_page(PAL_USER | flags);
}
