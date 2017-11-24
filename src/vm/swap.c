#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct bitmap* blocks;
static struct lock block_lock;

struct block *swap_table;

void 
swap_init(void)
{
	swap_table = block_get_role(BLOCK_SWAP);
	blocks = bitmap_create(block_size(swap_table)/PGSIZE*BLOCK_SECTOR_SIZE);
	lock_init(&block_lock);
}

// swap table --> page
void
swap_in(struct page* p)
{
	int i;
	lock_acquire(&block_lock);
	for(i=0;i<PGSIZE/BLOCK_SECTOR_SIZE;i++){
		block_read(swap_table, p->sector*PGSIZE/BLOCK_SECTOR_SIZE+i, 
				   p->kpage + i*BLOCK_SECTOR_SIZE);
	}
	bitmap_reset(blocks,p->sector);
	lock_release(&block_lock);

}

// page --> swap table
void 
swap_out(struct page* p)
{	
	int i;

	lock_acquire(&block_lock);
	p->sector = bitmap_scan_and_flip(blocks, 0, 1, false);
	for(i=0;i<PGSIZE/BLOCK_SECTOR_SIZE;i++)
		block_write(swap_table, p->sector*PGSIZE/BLOCK_SECTOR_SIZE+i, 
				   p->frame_entry->kpage + i*BLOCK_SECTOR_SIZE);
	lock_release(&block_lock);
}

void swap_reset(struct page* p){
	lock_acquire(&block_lock);
	bitmap_reset(blocks, p->sector);
	lock_release(&block_lock);

}
