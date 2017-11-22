#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "vm/frame.h"
#include "threads/malloc.h"

enum page_status
{
	ALL_ZERO,
	IN_FILESYS,
	IN_FRAME_TABLE,
	IN_SWAP_TABLE,
};

struct page
{
	void* addr;

	struct frame* frame_entry;
	enum page_status status;
	bool in_stack_page;

	struct hash_elem elem;
};


void page_table_init(struct hash* page_table);

void page_table_destroy(struct hash* page_table);

void add_page(struct thread* t, struct page* p);

unsigned page_hash_func(const struct hash_elem *e,void *aux);

bool page_less_func(const struct hash_elem *_a,
                	const struct hash_elem *_b,
                	void *aux);

#endif