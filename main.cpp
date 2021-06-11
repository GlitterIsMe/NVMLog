#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <cstring>

#include "libpmem.h"
#include "log.h"
#include "log_lb.h"

const uint64_t LOG_SIZE = 10 * 1024 * 1024;
const int ENTRY_SIZE = 64;

// param_1: The path to the NVM
// param_2: The size of NVM (GB)
// param_3: The number of logs
// param_4: The number of threads
// param_5: The number of operations
int main(int argc, char** argv) {
    // init the NVM space, create the log for the overall space;
    // init parameter: NVM path, map size, log num, thread num;
    if (argc != 7) {
        std::cout << "wrong parameter numbers\n";
        exit(-1);
    }

    std::string nvm_path(argv[1]);
    uint64_t map_size = atoi(argv[2]);
    int log_num = atoi(argv[3]);
    int thread_num = atoi(argv[4]);
    int op_num = atoi(argv[5]);
    int use_log_lb = atoi(argv[6]);

    size_t mapped_len = 0;
    int is_pmem = false;
#ifdef USE_PMEM
    char* base = (char*)pmem_map_file(nvm_path.c_str(), map_size * 1024 * 1024 * 1024,
                                      PMEM_FILE_CREATE, 0, &mapped_len, &is_pmem);
#elif defined(USE_DRAM)
    char* base = new char[map_size * 1024 * 1024 *1024];
#endif
    NVMLog total_log(base, 0, mapped_len);
    std::cout << "mapped len [" << mapped_len << "], is_pmem[" << is_pmem << "]\n";

    // create N log
    std::vector<NVMLoglb*> loglbs;
    std::vector<NVMLog*> logs;
    if (use_log_lb) {
        for (int i = 0; i < log_num; i++) {
            AllocRes res = total_log.Alloc(LOG_SIZE);
            if (res.first == SUCCESS) {
                std::cout << "Created log at [" << res.second << "]\n";
                loglbs.push_back(new NVMLoglb(base, res.second, LOG_SIZE));
            }
        }
    } else {
        for (int i = 0; i < log_num; i++) {
            AllocRes res = total_log.Alloc(LOG_SIZE);
            if (res.first == SUCCESS) {
                std::cout << "Created log at [" << res.second << "]\n";
                logs.push_back(new NVMLog(base, res.second, LOG_SIZE));
            }
        }
    }
    // launch N thread to write the log
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> rnd(0, log_num - 1);

    char* payload = new char[ENTRY_SIZE];
    memset((void*)payload, 0, ENTRY_SIZE);
    std::atomic_uint finished(0);

    int op_per_thread = op_num / thread_num;
    /*auto run_append = [&]{
        int cnt = 0;
        for (int i = 0; i < op_per_thread; i++) {
            // select a log in random;
            int log_seq = rnd(gen);
            // append 64 B to it
            NVMLog& target_log = logs[log_seq];
            std::pair<bool, uint64_t> res = target_log.Alloc(ENTRY_SIZE);
            if (res.first == SUCCESS) {
                target_log.Append(res.second, std::string(payload, ENTRY_SIZE));
                finished.fetch_add(1, std::memory_order_release);
        }
    };*/
    if (use_log_lb) assert(log_num == 1);

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < thread_num; ++i) {
        if (use_log_lb) {
            NVMLoglb* target_log = loglbs[0];
            threads.emplace_back(std::thread([&]{
                uint64_t offset = ENTRY_SIZE * i;
                for (int j = 0; j < op_per_thread; j++, offset + thread_num * ENTRY_SIZE) {
                    target_log->Append(offset, std::string(payload, ENTRY_SIZE));
                }}));
        } else {
            threads.emplace_back(std::thread([&]{
                int cnt = 0;
                for (int j = 0; j < op_per_thread; j++) {
                    // select a log in random;
                    int log_seq = rnd(gen);
                    // append 64 B to it
                    NVMLog* target_log = logs[log_seq];
                    std::pair<bool, uint64_t> res = target_log->Alloc(ENTRY_SIZE);
                    if (res.first == SUCCESS) {
                        target_log->Append(res.second, std::string(payload, ENTRY_SIZE));
                        //finished.fetch_add(1, std::memory_order_release);
                    }
                }}));
        }
    }
    for (int i = 0; i < thread_num; ++i) {
        threads[i].join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapse = end - start;

    std::cout << "finished " << op_num << " operations, throughput: " << op_num / elapse.count() << " MOPS\n";

    return 0;
}
