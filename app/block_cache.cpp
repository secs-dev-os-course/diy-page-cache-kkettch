#include "include/block_cache.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

CachedPage create_cached_page(off_t offset, size_t block_size, int fd) {
  CachedPage page;
  page.page_offset = offset;
  page.data.resize(block_size);
  page.is_changed = false;
  page.fd = fd;
  return page;
}

BlockCache create_block_cache(size_t block_size, size_t max_cache_size) {
  BlockCache cache;
  cache.block_size = block_size;
  cache.max_cache_size = max_cache_size;
  return cache;
}

int lab2_open(BlockCache *cache, const char *path) {
  int fd = ::open(path, O_RDWR | O_DIRECT);
  if (fd == -1) {
    perror("error during opening file");
    return -1;
  }

  struct stat file_stat;
  if (fstat(fd, &file_stat) == -1) {
    perror("error during stat");
    ::close(fd);
    return -1;
  }

  ino_t inode = file_stat.st_ino;
  cache->file_descriptors[fd] = fd;
  cache->fd_offsets[fd] = 0;
  cache->fd_to_inode[fd] = inode;
  return fd;
}

int lab2_close(BlockCache *cache, int fd) {
  if (cache->file_descriptors.find(fd) == cache->file_descriptors.end()) {
    std::cerr << "error: file is not opened\n";
    return -1;
  }

  int ret = lab2_fsync(cache, fd);
  if (ret == -1) {
    std::cerr << "data sync error when closing file\n";
    return -1;
  }

  ret = ::close(fd);
  if (ret == -1) {
    perror("error during closing file");
    return -1;
  }

  ino_t inode = cache->fd_to_inode[fd];
  cache->fd_to_inode.erase(fd);
  cache->file_descriptors.erase(fd);
  cache->fd_offsets.erase(fd);

  return 0;
}

void evict_page(BlockCache *cache) {
  if (cache->cache.empty()) {
    return;
  }

  auto &page_to_evict = cache->cache.front();

  if (page_to_evict.is_changed) {
    ssize_t ret = ::pwrite(page_to_evict.fd, page_to_evict.data.data(), cache->block_size, page_to_evict.page_offset);
    if (ret == -1) {
      std::cerr << "error writing page to disk during eviction.\n";
      return;
    }
    page_to_evict.is_changed = false;
  }

  ino_t inode = cache->fd_to_inode[page_to_evict.fd];
  cache->inode_cache_map[inode].erase(page_to_evict.page_offset);
  cache->cache_map.erase(page_to_evict.page_offset);
  cache->cache.pop_front();
}

bool is_valid_fd(BlockCache *cache, int fd) {
  if (cache->file_descriptors.find(fd) == cache->file_descriptors.end()) {
    std::cerr << "error: incorrect fd\n";
    return false;
  }
  return true;
}

ssize_t lab2_read(BlockCache *cache, int fd, void *buf, size_t count) {
  if (!is_valid_fd(cache, fd)) {
    return -1;
  }

  off_t offset = cache->fd_offsets[fd];
  size_t bytes_read = 0;
  ino_t inode = cache->fd_to_inode[fd];

  while (bytes_read < count) {
    off_t block_offset = (offset / cache->block_size) * cache->block_size;
    size_t page_offset = offset % cache->block_size;
    size_t bytes_to_read = std::min(count - bytes_read, cache->block_size - page_offset);

    auto it = cache->inode_cache_map[inode].find(block_offset);
    if (it != cache->inode_cache_map[inode].end()) {
      auto &page = *it->second;
      std::memcpy((char *)buf + bytes_read, page.data.data() + page_offset, bytes_to_read);
    } else {
      CachedPage page = create_cached_page(block_offset, cache->block_size, fd);

      void *aligned_buf = aligned_alloc(cache->block_size, cache->block_size);

      if (!aligned_buf) {
        std::cerr << "error during memory allocation\n";
        return -1;
      }

      ssize_t ret = ::pread(fd, aligned_buf, cache->block_size, block_offset);

      if (ret == 0) break;  
      if (ret == -1) {
        free(aligned_buf);
        perror("error reading");
        return -1;
      }

      std::memcpy(page.data.data(), aligned_buf, cache->block_size);
      free(aligned_buf);

      if (cache->cache.size() >= cache->max_cache_size) {
        evict_page(cache);
      }

      page.is_changed = false;
      cache->cache.push_back(std::move(page));
      cache->cache_map[block_offset] = std::prev(cache->cache.end());
      cache->inode_cache_map[inode][block_offset] = std::prev(cache->cache.end());

      std::memcpy((char *)buf + bytes_read, cache->cache.back().data.data() + page_offset, bytes_to_read);
    }

    bytes_read += bytes_to_read;
    offset += bytes_to_read;
  }

  cache->fd_offsets[fd] = offset;
  return bytes_read;
}

ssize_t lab2_write(BlockCache *cache, int fd, const void *buf, size_t count) {

  if (!is_valid_fd(cache, fd)) {
    return -1;
  }

  off_t offset = cache->fd_offsets[fd];
  size_t bytes_written = 0;
  ino_t inode = cache->fd_to_inode[fd];

  while (bytes_written < count) {

    off_t block_offset = (offset / cache->block_size) * cache->block_size;
    size_t page_offset = offset % cache->block_size;
    size_t bytes_to_write = std::min(count - bytes_written, cache->block_size - page_offset);
    auto it = cache->inode_cache_map[inode].find(block_offset);

    if (it == cache->inode_cache_map[inode].end()) {

      CachedPage page = create_cached_page(block_offset, cache->block_size, fd);

      void *aligned_buf = aligned_alloc(cache->block_size, cache->block_size);

      if (!aligned_buf) {
        std::cerr << "error during memory allocation\n";
        return -1;
      }

      ssize_t ret = ::pread(fd, aligned_buf, cache->block_size, block_offset);

      if (ret == -1) {
        free(aligned_buf);
        perror("error reading during writing");
        return -1;
      }

      std::memcpy(page.data.data(), aligned_buf, cache->block_size);
      free(aligned_buf);

      if (cache->cache.size() >= cache->max_cache_size) {
        evict_page(cache);
      }

      page.is_changed = false;
      cache->cache.push_back(std::move(page));
      cache->cache_map[block_offset] = --cache->cache.end();
      cache->inode_cache_map[inode][block_offset] = --cache->cache.end();
    }

    auto &page = *(it == cache->inode_cache_map[inode].end() ? --cache->cache.end() : it->second);
    std::memcpy(page.data.data() + page_offset, (char *)buf + bytes_written, bytes_to_write);
    page.is_changed = true;

    bytes_written += bytes_to_write;
    offset += bytes_to_write;
  }

  cache->fd_offsets[fd] = offset;
  return bytes_written;
}

off_t lab2_lseek(BlockCache *cache, int fd, off_t offset, int whence) {

  if (!is_valid_fd(cache, fd)) {
    return -1;
  }

  off_t new_offset = ::lseek(fd, offset, whence);
  if (new_offset == -1) {
    perror("error lseek");
    return -1;
  }

  cache->fd_offsets[fd] = new_offset;
  return new_offset;
}

int lab2_fsync(BlockCache *cache, int fd) {

  if (!is_valid_fd(cache, fd)) {
    return -1;
  }

  ino_t inode = cache->fd_to_inode[fd];

  for (auto it = cache->inode_cache_map[inode].begin(); it != cache->inode_cache_map[inode].end(); ++it) {
    auto &page = *(it->second);
    if (page.is_changed) {
      ssize_t ret = ::pwrite(fd, page.data.data(), cache->block_size, page.page_offset);
      if (ret == -1) {
        perror("error writing during fsync");
        return -1;
      }
      page.is_changed = false;
    }
  }

  return 0;
}
