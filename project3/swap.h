#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>

void swap_init(void);
size_t swap_out(void* kpage);
void swap_in(void* kpage, size_t swap_index);
void swap_free(size_t swap_index);
#endif /* vm/swap.h */
