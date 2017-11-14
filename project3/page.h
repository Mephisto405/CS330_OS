#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "filesys/file.h"
#include <hash.h>
#include "threads/thread.h"
#include "vm/frame.h"

enum page_status
{
    FILE,						/* When page fault, find this page in file system */
    SWAP						/* When page fault, fine this page in swap table */
};

struct page{
	/* Basic */
	void* upage;				/* User virtual page */
	bool loaded;
	enum page_status type;
	/* File */
	struct file* file;
	bool writable;				/* whether the file is writable or not */
	off_t ofs;
	size_t read_bytes;
	size_t zero_bytes;
	/* Swap */
	size_t swap_index;
	bool swapped;
	/* Synch */
	bool pinned;

	struct hash_elem hash_elem;
};

void page_init(struct hash* page_hash_map);
struct page* page_find (void *upage);
bool page_load_from_file(struct page* page);
bool page_load_from_swap(struct page* page);
bool page_stack_growth(void* upage);
struct page* init_page(void* upage);
void page_destroy(void);

#endif /* vm/page.h */
