#include "vm/swap.h"
#include <bitmap.h>
#include "devices/disk.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <stdbool.h>
#include <stdint.h>
#include <debug.h>

#define NUM_SECTORS PGSIZE/DISK_SECTOR_SIZE

static struct bitmap* swap_bitmap;			/* Tracking swap slots */
static struct lock swap_lock_bitmap;		/* Lock for swap_bitmap */
static struct disk* swap_disk;				  /* Swap disk */
static void swaplock_acquire(void);			/* swaplock acquire */
static void swaplock_release(void);			/* swaplock release */

/*
 * Should make interface between swap disk and frame.c
 */



/* Initialize swap system */
void 
swap_init(void)								
{
	lock_init(&swap_lock_bitmap);
	swap_disk = disk_get(1,1);
	swap_bitmap = bitmap_create(disk_size(swap_disk)*DISK_SECTOR_SIZE/PGSIZE);
}

static void
swaplock_acquire(void)
{
	lock_acquire(&swap_lock_bitmap);
}

static void
swaplock_release(void)
{
	lock_release(&swap_lock_bitmap);
}

/* Swap the frame KPAGE into a swap slot */
size_t
swap_out(void* kpage)						
{
	size_t swap_index;
	disk_sector_t i, start_write;
	
	swaplock_acquire();
	swap_index = bitmap_scan_and_flip(swap_bitmap, 0, 1, false);
	if( swap_index == BITMAP_ERROR ){
		PANIC("swap_out: out of slots"); // not reached
	}
	
	start_write = NUM_SECTORS * swap_index;
	ASSERT(swap_disk!=NULL);
	for( i = 0; i < NUM_SECTORS; i++ ){
		disk_write(swap_disk, start_write + i, kpage + (DISK_SECTOR_SIZE * i));
	}
	swaplock_release();
	return swap_index;
}

/* Swap the frame KPAGE at SWAP_INDEX out of a swap slot */
void
swap_in(void* kpage, size_t swap_index)		
{
 	// for debug
	ASSERT(bitmap_test(swap_bitmap, swap_index));
	disk_sector_t i, start_read;
	
  swaplock_acquire(); // DEBUGPOINT
	start_read = NUM_SECTORS * swap_index;
	for( i = 0; i < NUM_SECTORS; i++ ){
		disk_read(swap_disk, start_read + i, kpage + (DISK_SECTOR_SIZE * i));
	}
  bitmap_set(swap_bitmap, swap_index, false);
  swap_index = (size_t)(-1);
  swaplock_release();
}

void
swap_free(size_t swap_index)
{
  swaplock_acquire();
  bitmap_set(swap_bitmap, swap_index, false);
  swaplock_release();
}
