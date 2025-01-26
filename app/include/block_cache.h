#pragma once

#include <iostream>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <list>
#include <sys/stat.h>

struct CachedPage {
  off_t page_offset;
  std::vector<char> data;
  bool is_changed;
  int fd;
};

struct BlockCache {
  size_t block_size;                   
  size_t max_cache_size;                
  std::unordered_map<off_t, std::list<CachedPage>::iterator> cache_map; 
  std::list<CachedPage> cache;   
  std::unordered_map<int, off_t> fd_offsets;
  std::unordered_map<int, int> file_descriptors; 
};

CachedPage create_cached_page(off_t offset, size_t block_size, int fd);
BlockCache create_block_cache(size_t block_size, size_t max_cache_size);
int lab2_open(BlockCache *cache, const char *path);
int lab2_close(BlockCache *cache, int fd);
void evict_page(BlockCache *cache);
ssize_t lab2_read(BlockCache *cache, int fd, void *buf, size_t count);
ssize_t lab2_write(BlockCache *cache, int fd, const void *buf, size_t count);
off_t lab2_lseek(BlockCache *cache, int fd, off_t offset, int whence);
int lab2_fsync(BlockCache *cache, int fd);