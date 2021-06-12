#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <random>
#include <chrono>
#include <cstring>
#include <cassert>

#include <gflags/gflags.h>

#include "libpmem.h"
#include "log.h"
#include "log_lb.h"

DEFINE_string(nvm_path, "/pmem0/log/test_file", "The path of pmem");
DEFINE_uint64(nvm_size, 10, "The size of pmem <in GB>");
DEFINE_int32(num_logs, 1, "The number of log");
DEFINE_int32(num_threads, 1, "The number of thread");
DEFINE_uint64(nums, 1000000, "The number of operations");
DEFINE_bool(use_atomic_log, false, "Whether use the atomic based log");

DEFINE_int32(value_size, 64, "The size of log entries");
DEFINE_int64(log_size, 1024 * 1024 * 1024, "The size of log");

DEFINE_bool(single_log, false, "Whether use a single log");

int main(int argc, char** argv) {
    google::ParseCommandLineFlags(&argc, &argv, true);

    int log_num = FLAGS_num_logs;
    int value_size = FLAGS_value_size;
    int thread_num = FLAGS_num_threads;

    size_t mapped_len = 0;
    int is_pmem = false;
#ifdef USE_PMEM
    char* base = (char*)pmem_map_file(FLAGS_nvm_path.c_str(), FLAGS_nvm_size * 1024 * 1024 * 1024,
                                      PMEM_FILE_CREATE, 0, &mapped_len, &is_pmem);
#elif defined(USE_DRAM)
    char* base = new char[FLAGS_nvm_size * 1024 * 1024 *1024];
#endif

    NVMLog total_log(base, 0, mapped_len);
    std::cout << "mapped len [" << mapped_len << "], is_pmem[" << is_pmem << "]\n";

    // create N log
    std::vector<NVMLoglb*> loglbs;
    std::vector<NVMLog*> logs;
    if (!FLAGS_use_atomic_log) {
        for (int i = 0; i < log_num; i++) {
            AllocRes res = total_log.Alloc(FLAGS_log_size);
            if (res.first == SUCCESS) {
                std::cout << "Created log at [" << res.second << "]\n";
                loglbs.push_back(new NVMLoglb(base, res.second, FLAGS_log_size));
            }
        }
    } else {
        for (int i = 0; i < log_num; i++) {
            AllocRes res = total_log.Alloc(FLAGS_log_size);
            if (res.first == SUCCESS) {
                std::cout << "Created log at [" << res.second << "]\n";
                logs.push_back(new NVMLog(base, res.second, FLAGS_log_size));
            }
        }
    }

    // launch N thread to write the log
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> rnd(0, log_num - 1);

    // init log entry
    char* payload = new char[value_size];
    memset((void*)payload, 0, value_size);

    int op_per_thread = FLAGS_nums / thread_num;
    if (!FLAGS_use_atomic_log) assert(log_num == 1);

    std::vector<std::thread> threads;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < thread_num; ++i) {
        if (!FLAGS_use_atomic_log) {
            NVMLoglb* target_log = loglbs[0];
            int thread_count = i;
            threads.emplace_back(std::thread([&]{
                uint64_t offset = value_size * thread_count;
                for (int j = 0; j < op_per_thread; j++, offset + thread_num * value_size) {
                    target_log->Append(offset, std::string(payload, value_size));
                }}));
        } else if (FLAGS_single_log) {
            NVMLog* target_log = logs[0];
            threads.emplace_back(std::thread([&]{
                for (int j = 0; j < op_per_thread; j++) {
                    std::pair<bool, uint64_t> res = target_log->Alloc(value_size);
                    if (res.first == SUCCESS) {
                        target_log->Append(res.second, std::string(payload, value_size));
                    } else {
                        std::cout << "Alloc failed, quit\n";
                        return;
                    }
                }}));
        } else {
            threads.emplace_back(std::thread([&]{
                for (int j = 0; j < op_per_thread; j++) {
                    // select a log in random;
                    int log_seq = rnd(gen);
                    // append 64 B to it
                    NVMLog* target_log = logs[log_seq];
                    std::pair<bool, uint64_t> res = target_log->Alloc(value_size);
                    if (res.first == SUCCESS) {
                        target_log->Append(res.second, std::string(payload, value_size));
                    } else {
                        std::cout << "Alloc failed, quit\n";
                        return;
                    }
                }}));
        }
    }
    for (int i = 0; i < thread_num; ++i) {
        threads[i].join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapse = end - start;

    std::cout << "finished " << FLAGS_nums << " operations, throughput: " << FLAGS_nums / elapse.count() << " MOPS\n";

    return 0;
}
