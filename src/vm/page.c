#include "vm/page.h"
#include <debug.h>

#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"

void
page_table_init(struct hash* page_table)
{
	hash_init(page_table, page_hash_func, page_less_func, NULL);
}

void
page_table_destroy(struct hash* page_table)
{
	struct hash_iter iter;
	while(hash_next(&iter)){
		struct page* page_temp = hash_entry(hash_cur(&iter), struct page, elem);
		if(page_temp->frame_entry){
			free_frame(page_temp->frame_entry);
			page_temp->frame_entry = NULL;
		}
		if(page_temp->sector != -1)
			swap_reset(page_temp);
		free(page_temp);
	}
}

void
add_page(struct thread* t, struct page* p)
{
	hash_insert(&t->page_table, &p->elem);
}

struct page*
page_table_lookup(void* address)
{
	struct thread* curr_thread = thread_current();
	struct page target;
	struct hash_elem *e;

	target.addr = (void*)(((uintptr_t)address>>PGBITS)<<PGBITS); //page start addr
	e= hash_find(&curr_thread->page_table, &target.elem);
	return e==NULL ? NULL : hash_entry(e, struct page, elem);
}


unsigned
page_hash_func(const struct hash_elem *e,void *aux UNUSED)
{
	const struct page* p = hash_entry(e, struct page, elem);
	return hash_bytes(&p->addr, sizeof(p->addr));
}

bool 
page_less_func(const struct hash_elem *_a,
                const struct hash_elem *_b,
                void *aux UNUSED)
{
	struct page* a = hash_entry(_a, struct page, elem);
	struct page* b = hash_entry(_b, struct page, elem);
	return a->addr < b->addr;
}

