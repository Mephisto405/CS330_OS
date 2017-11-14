#include "vm/page.h"
#include "userprog/syscall.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "threads/vaddr.h"
#include <hash.h>
#include <stdbool.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include <string.h>
#include "userprog/syscall.h"
#include "threads/interrupt.h"

static hash_hash_func page_hash_func;
static hash_less_func page_less_func;
static hash_action_func page_destructor;

/*
 * Supplemental page system.
 * Not real but abstraction.
 * Should provide a scheme or information what page does a thread have.
 * Using frame.c
 * Each thread has a supplemental page table.
 */

void
page_init(struct hash* page_hash_map)
{
	if( !hash_init(page_hash_map, page_hash_func, page_less_func, NULL) ){
		syscall_exit(-1);
	}
}

struct page* page_find (void *upage)
{
	struct page page;
	struct hash_elem* e;
	
	page.upage = upage;
	e = hash_find(&thread_current()->page_hash_map, &page.hash_elem);

	if( e != NULL ){
		return hash_entry(e, struct page, hash_elem);
	}
	else{
		return NULL;
	}
}

bool page_load_from_file(struct page* page)
{
  void* upage;
  void* kpage;
  bool success = false;
  int readed;
	
  upage = page->upage;
  /* Zero page */
  if(page->read_bytes == 0){
    kpage = frame_get_without_pagedir_set(page, PAL_ZERO);
    if( kpage == NULL ){
    	return false;
    }
  }
  /* Non-zero page */
  else{
    kpage = frame_get_without_pagedir_set(page, 0);
    if( kpage == NULL ){
    	return false;
    }
    /* Set non-zero file read bytes */
    filelock_acquire();
    readed = file_read_at(page->file, kpage, page->read_bytes, page->ofs);
    if( readed != page->read_bytes ){
        filelock_release();
        frame_free_without_pagedir_clear(kpage);
        return false;
    }
    filelock_release();
    /* Set remaining zero bytes */
    memset(kpage + page->read_bytes, 0, page->zero_bytes);
  }
  /* Install page */
  success = install_page(upage, kpage, page->writable);
  if( success ){
	page->loaded = true;
  }
  else{
    frame_free_without_pagedir_clear(kpage);
  }
  return success;
}

bool page_load_from_swap(struct page* page)
{
	void* kpage;
	bool success;
	
	kpage = frame_get_without_pagedir_set(page, 0);
	if( kpage == NULL ){
		return false;
	}
		
	success = install_page(page->upage, kpage, page->writable);
	if( success ){
		swap_in(page->upage, page->swap_index);
    page->swapped = false;
	}
	else{
		frame_free_without_pagedir_clear(kpage);
	}
	return success;
}

bool page_stack_growth(void* upage)
{
	void* kpage;
	bool success;
	struct page* page;
	/* Make new supplemental page(just a skeleton of page) */
	page = init_page(upage);
	page->loaded = true;
	page->writable = true;
	page->type = SWAP;
	page->pinned = true;
  /* Get new frame  */
  kpage = frame_get_without_pagedir_set(page, PAL_ZERO);
  if( kpage == NULL) {
    free(page);
    return false;
  }
	/* Install page */
	success = install_page(upage, kpage, true);
	if( success ){
		if( intr_context() ){
			page->pinned = false;
		}
		return (hash_insert(&thread_current()->page_hash_map, &page->hash_elem) == NULL);
	}
	else{
		free(page);
		frame_free_without_pagedir_clear(kpage);
	}

	return success;
}

struct page* init_page(void* upage)
{
	struct page* page = (struct page*)malloc(sizeof(struct page));
	
	page->upage = upage;
	page->loaded = false;
	page->file = NULL;
  //page->swap_index = (size_t)(-1);
	
	return page;
}
//===================================================================
//===================================================================
void
page_destroy(void)
{
	hash_destroy(&thread_current()->page_hash_map, page_destructor);
}

/* Return a hash value for page P. */
static unsigned
page_hash_func(const struct hash_elem *p, void *aux UNUSED){
	const struct page *P = hash_entry(p, struct page, hash_elem);
  	return hash_bytes(&P->upage, sizeof(P->upage));
}

/* Return true if page P precedes page Q. */
static bool
page_less_func(const struct hash_elem *p, const struct hash_elem *q,
           void *aux UNUSED)
{
 	 const struct page *P = hash_entry(p, struct page, hash_elem);
 	 const struct page *Q = hash_entry(q, struct page, hash_elem);

 	 return (P->upage) < (Q->upage);
}

/* Free a page. */
static void
page_destructor(struct hash_elem *e, void *aux UNUSED)
{
	struct page *page;
	void *kpage;

	page = hash_entry(e, struct page, hash_elem);
	kpage = pagedir_get_page(thread_current()->pagedir, page->upage);
	if(kpage != NULL && page->loaded == true)
	{
		frame_free_without_pagedir_clear(kpage);
		pagedir_clear_page(thread_current()->pagedir, page->upage);
	}
  
  if( page->file != NULL && page->type == SWAP && page->swapped == true ){
    swap_free(page->swap_index);
  }
  
	free(page);
	// DEBUGPOINT: swap free???
}
