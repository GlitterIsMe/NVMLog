//
// Created by 張藝文 on 2021/6/4.
//

#ifndef NVM_LOG_LOG_LB_H
#define NVM_LOG_LOG_LB_H

#include <cstdio>
#include <string>
#include <atomic>
#include <mutex>
#include <libpmem.h>

class NVMLoglb {
public:
    NVMLoglb(const char* base, uint64_t start_offset, uint64_t size);
    ~NVMLoglb();

    void Append(const std::string src);
    void lock() {mu_.lock();};
    void unlock() {mu_.unlock();};

private:
    const char* base_;
    const uint64_t start_;
    const uint64_t size_;
    unsigned int cur_;
    std::mutex mu_;
};

#endif //NVM_LOG_LOG_LB_H
