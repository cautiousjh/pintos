#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"

#define CACHE_SIZE_MAX 64

void cache_init();
void cache_read(block_sector_t, void*);
void cache_write(block_sector_t, void*);

#endif /* filesys/file.h */