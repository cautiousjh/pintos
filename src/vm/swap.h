#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"
#include "vm/page.h"

void swap_init(void);

void swap_in(struct page* p);
void swap_out(struct page* p);
void swap_reset(struct page* p);

#endif