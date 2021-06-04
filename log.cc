//
// Created by 張藝文 on 2021/6/3.
//

#include "log.h"

NVMLog::NVMLog(const char *base, uint64_t start_offset, uint64_t size)
    :base_(base), start_(start_offset), size_(size){}

NVMLog::~NVMLog() {

}

AllocRes NVMLog::Alloc(uint64_t alloc_size) {
    if (cur_.load(std::memory_order_acquire) + alloc_size >= size_) {
        return AllocRes (FULL, 0);
    }
    bool alloc_success = false;
    unsigned int cur = 0;
    int try_times = 0;
    do {
        cur = cur_.load(std::memory_order_acquire);
        alloc_success = cur_.compare_exchange_weak(cur, cur + alloc_size,
                                              std::memory_order_release);
        try_times++;
        if (try_times > 10) {
            return AllocRes (FAILED, 0);
        }
    } while (!alloc_success);
    return AllocRes (SUCCESS, cur);
}

void NVMLog::Append(uint64_t offset, const std::string src) {
    pmem_memcpy_persist((void*)(base_ + offset), (void*)(src.c_str()), src.size());
}