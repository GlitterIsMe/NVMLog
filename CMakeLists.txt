cmake_minimum_required(VERSION 3.16)
project(NVM_log)

set(CMAKE_CXX_STANDARD 14)

include(FindThreads)
#find_package(PkgConfig QUIET)
#pkg_check_modules(LIBPMEM REQUIRED libpmem)

find_package(LIBPMEM)

find_package(GFLAGS REQUIRED)

add_definitions(-DUSE_PMEM)
#add_definitions(-DUSE_DRAM)

add_executable(NVM_log "")
target_sources(NVM_log PRIVATE
        "log.cc"
        "log.h"
        "log_lb.cc"
        "log_lb.h"
        "main.cpp")

target_include_directories(NVM_log PUBLIC ${LIBPMEM_INCLUDE_DIRS} ${GFLAGS_INCLUDE_DIRS})
target_link_libraries(NVM_log ${LIBPMEM_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${GFLAGS_LIBRARIES})

if (LIBPMEM_FOUND)
    target_include_directories(NVM_log PUBLIC ${LIBPMEM_INCLUDE_DIRS})
    target_link_libraries(NVM_log ${LIBPMEM_LIBRARIES})
endif ()