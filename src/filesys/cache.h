#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/block.h"

#define CACHE_SIZE_MAX 64
#define WRITE_BACK_PERIOD 1024

void cache_init(void);
void cache_read(block_sector_t, void*);
void cache_write(block_sector_t, void*);
void cache_flush(void);

#endif /* filesys/file.h */