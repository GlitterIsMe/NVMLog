//
// Created by 張藝文 on 2021/6/4.
//

#include "log_lb.h"

NVMLoglb::NVMLoglb(const char *base, uint64_t start_offset, uint64_t size)
        :base_(base), start_(start_offset), size_(size){}

NVMLoglb::~NVMLoglb() {

}

void NVMLoglb::Append(const std::string& src) {
    pmem_memcpy_persist((void*)(base_ + cur_), (void*)(src.c_str()), src.size());
    cur_ += src.size();
}

void NVMLoglb::Append(uint64_t offset, const std::string& src) {
    pmem_memcpy_persist((void*)(base_ + offset), (void*)(src.c_str()), src.size()));
}