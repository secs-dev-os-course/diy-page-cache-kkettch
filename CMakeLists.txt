cmake_minimum_required(VERSION 3.12)

project(BlockCache)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(LIB_SOURCES
    app/block_cache.cpp
    app/io_thpt_read.cpp
)
set(BENCH_SOURCES
    app/app.cpp
)

include_directories(app/include)

add_library(library SHARED ${LIB_SOURCES})
target_include_directories(library PUBLIC include)

add_executable(test_bench ${BENCH_SOURCES})
target_link_libraries(test_bench PRIVATE library)

set(CMAKE_BUILD_TYPE Debug) # Ensures the build type is Debug
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g") # Adds debug symbols and disables optimizations
