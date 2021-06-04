cmake_minimum_required(VERSION 3.17)
project(NVM_log)

set(CMAKE_CXX_STANDARD 14)

find_library(PMEM libpmem REQUIRED)

add_executable(NVM_log "")
target_sources(NVM_log PRIVATE
        "log.cc"
        "log.h"
        "main.cpp")

target_link_libraries(NVM_log PMEM phtread)