#include "vm/swap.h"
#include <bitmap.h>
#include "devices/block.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static struct bitmap* blocks;
static struct lock block_lock;

struct block *swap_table;

void 
swap_init(void)
{
	blocks = bitmap_create(1<<9);
	lock_init(&block_lock);
	swap_table = block_get_role(BLOCK_SWAP);
}

// swap table --> page
void
swap_in(struct page* p)
{
	uint32_t tmp_addr = p->frame_entry->kpage;
	int sector, i;

	lock_acquire(&block_lock);
	p->sector = bitmap_scan_and_flip(blocks, 0, 1, false);
	for(i=0;i<8;i++){
		block_write(swap_table, i+sector*8, (void*)tmp_addr);
		tmp_addr = tmp_addr + BLOCK_SECTOR_SIZE;
	}
	lock_release(&block_lock);

}

// page --> swap table
void 
swap_out(struct page* p)
{	
	uint32_t tmp_addr = p->frame_entry->kpage;
	int sector = p->sector, i;

	lock_acquire(&block_lock);
	for(i=0;i<8;i++){
		block_read(swap_table, i+sector*8, (void*)tmp_addr);
		tmp_addr = tmp_addr + BLOCK_SECTOR_SIZE;
	}
	bitmap_reset(blocks,sector);
	lock_release(&block_lock);
}

void swap_reset(struct page* p){

}
