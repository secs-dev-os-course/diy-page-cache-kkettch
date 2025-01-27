#include <iostream>
#include <chrono>
#include <cstdlib>
#include "include/io_thpt_read.h" 

int main() {
    const std::string file_path = "benchmark_file.bin";

    int repeat_count = 10;

    for (int i = 0; i < repeat_count; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        IoThptReadWithCache(file_path);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed_time = end - start;
        std::cout << "Время с кэшем: " << elapsed_time.count() << " ms" << std::endl;
    }

    // for (int i = 0; i < repeat_count; ++i) {
    //     auto start = std::chrono::high_resolution_clock::now();
    //     IoThptReadWithoutCache(file_path);  
    //     auto end = std::chrono::high_resolution_clock::now();
    //     std::chrono::duration<double, std::milli> elapsed_time = end - start;
    //     std::cout << "Время без кэша: " << elapsed_time.count() << " ms" << std::endl;
    // }

    return 0;
}