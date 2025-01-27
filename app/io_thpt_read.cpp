#include "include/block_cache.h"
#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>

#define BLOCK_SIZE (16 * 1024)  // 16 KB

BlockCache cache = create_block_cache(BLOCK_SIZE, 512);

void IoThptReadWithCache(const std::string &file_path) {

  char buffer[BLOCK_SIZE / 4];

  int fd = lab2_open(&cache, file_path.c_str());
  if (fd == -1) {
    std::cerr << "[ERROR] Ошибка открытия файла: " << file_path << std::endl;
    return;
  }

  ssize_t bytes_read;
  size_t total_bytes_read = 0;

  while ((bytes_read = lab2_read(&cache, fd, buffer, BLOCK_SIZE / 4)) > 0) {
    total_bytes_read += bytes_read;
  }

  if (bytes_read == -1) {
    perror("[ERROR] Ошибка чтения файла");
  }

  if (lab2_close(&cache, fd) == -1) {
    std::cerr << "[ERROR] Ошибка закрытия файла: " << file_path << std::endl;
  }

}

void IoThptReadWithoutCache(const std::string &file_path) {
  char buffer[BLOCK_SIZE / 4];

  int fd = open(file_path.c_str(), O_RDONLY | O_DIRECT);
  if (fd == -1) {
    perror("[ERROR] Ошибка открытия файла");
    return;
  }

  ssize_t bytes_read;
  size_t total_bytes_read = 0;

  while ((bytes_read = read(fd, buffer, BLOCK_SIZE / 4)) > 0) {
    total_bytes_read += bytes_read;
  }

  if (bytes_read == -1) {
    perror("[ERROR] Ошибка чтения файла");
  }

  if (close(fd) == -1) {
    perror("[ERROR] Ошибка закрытия файла");
  }
}