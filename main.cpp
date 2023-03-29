#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <cstring>
#include <cassert>

#include <gflags/gflags.h>
#include <future>


#include "hash.h"
#include "libpmem.h"
#include "histogram.h"

DEFINE_string(nvm_path, "/mnt/pmem0/test_file", "The path of pmem");
DEFINE_uint64(nvm_size, 10, "The size of pmem <in GB>");
DEFINE_int32(num_threads, 1, "The number of thread");
//DEFINE_uint64(nums, 1000000, "The number of operations");
DEFINE_uint32(io_size, 1, "The total size of IO that each thread will perform");

DEFINE_int32(block_size, 16, "The size of log entries");

//#define COUNT_LAT
leveldb::Histogram hist;

inline unsigned long long rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
    return ((unsigned long long)hi << 32) | lo;
}

inline unsigned long long start() {
    return rdtsc();
}

inline double get_lat_us(unsigned long long start) {
    return (double)(rdtsc() - start) / 2.1e6;
}

int main(int argc, char** argv) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    printf("threads, block_size\n");
    printf("%d, %d\n", FLAGS_num_threads, FLAGS_block_size);
    hist.Clear();

    int block_size = FLAGS_block_size;
    int thread_num = FLAGS_num_threads;

    size_t mapped_len = 0;
    int is_pmem = false;
#ifdef USE_PMEM
    char* base = (char*)pmem_map_file(FLAGS_nvm_path.c_str(), FLAGS_nvm_size * 1024 * 1024 * 1024,
                                      PMEM_FILE_CREATE, 0, &mapped_len, &is_pmem);
#elif defined(USE_DRAM)
    char* base = new char[FLAGS_nvm_size * 1024 * 1024 *1024];
#endif
    if (base == nullptr) {
        printf("Map failed for [%s]\n", strerror(errno));
        exit(-1);
    }
    std::cout << "mapped len [" << mapped_len << "], is_pmem[" << is_pmem << "]\n";

    uint64_t num_blocks = mapped_len / block_size - 1;

    struct thread_stat {
        uint64_t op_;
        uint64_t start_;
        uint64_t num_blocks_;
        int id_;
        char* payload{nullptr};
        leveldb::Histogram* hist{nullptr};

        thread_stat(int op, uint64_t start, uint64_t nums, int id) {
            op_ = op;
            start_ = start;
            num_blocks_ = nums;
            id_ = id;
        }
    };

    uint64_t per_threads_block_num = num_blocks / FLAGS_num_threads;
    std::vector<thread_stat> stats;
    for (int i = 0; i < FLAGS_num_threads; ++i) {
        stats.emplace_back(FLAGS_io_size * 1024 * 1024 * 1024UL / FLAGS_block_size, i * per_threads_block_num, per_threads_block_num, i);
        stats[i].payload = new char[FLAGS_block_size];
        memset((void*)stats[i].payload, 'a', FLAGS_block_size);
#ifdef COUNT_LAT
        stats[i].hist = &hist;
#endif
    }

    auto work = [=](thread_stat *stat){
        for (int i = 0; i < stat->op_; ++i) {
            int id = i;
            size_t rand_id = hash::hash_funcs[0](&id, sizeof(id), 0xc70697UL+stat->id_);
            int rand_blk_id = rand_id % stat->num_blocks_;
            //if ((base + (stat->start_ + rand_blk_id) * FLAGS_block_size) > (base + mapped_len)) printf("overflow\n");
#ifdef COUNT_LAT
            auto t1 = start();
            pmem_memcpy_persist(base + (stat->start_ + rand_blk_id) * FLAGS_block_size, stat->payload, FLAGS_block_size);
            hist.Add(get_lat_us(t1));
#else
            pmem_memcpy_persist(base + (stat->start_ + rand_blk_id) * FLAGS_block_size, stat->payload, FLAGS_block_size);
#endif


        }
        return 0;
    };

    std::vector<std::future<int>> ft;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < thread_num; ++i) {
        ft.push_back(std::async(work, &stats[i]));
    }
    for (int i = 0; i < thread_num; ++i) {
        ft[i].get();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapse = end - start;

    std::cout << "each thread finished " << FLAGS_io_size << " GB, throughput: " << (double)FLAGS_io_size * 1024 * 1024 * FLAGS_num_threads / FLAGS_block_size / elapse.count() << " MOPS\n";
    std::cout << "bandwidth: " << (FLAGS_io_size * FLAGS_num_threads * 1024)/*MB*/ / (elapse.count() / 1000000) << " MB/s\n";
    printf("%s\n", hist.ToString().c_str());

    for (int i = 0; i < thread_num; ++i) {
        delete[] stats[i].payload;
    }
    pmem_unmap(base, mapped_len);
    return 0;
}
