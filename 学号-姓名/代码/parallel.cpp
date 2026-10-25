#include "sobel.hpp"
#include <omp.h>

namespace {
// 诊断代码单独实例化，正式计时不更新计数器或记录线程时间。
template<Method method, bool diagnose>
void execute(const Image& in, Image& out, int requested, int block_rows,
             Diagnostics* stats) {
    check_output(in, out);
    if (requested < 1) throw std::runtime_error("threads must be positive");
    const int total = task_count(in, block_rows);
    int next = 0;
    omp_lock_t lock;
    if constexpr (method == Method::Lock) omp_init_lock(&lock);
    sobel_row(in, out, 0);
    if (in.height > 1) sobel_row(in, out, in.height - 1);

    #pragma omp parallel default(none) shared(in, out, next, lock, stats) firstprivate(requested, block_rows, total) num_threads(requested)
    {
        const int id = omp_get_thread_num();
        const int team = omp_get_num_threads();
        double start = 0;
        ThreadStats local;
        if constexpr (diagnose) {
            start = omp_get_wtime();
            if (id == 0) stats->actual_threads = team;
        }
        auto process = [&](int task) {
            sobel_block(in, out, task, block_rows);
            if constexpr (diagnose) {
                // 原子计数让重复领取也能被可靠地检查出来。
                #pragma omp atomic update
                stats->visits[task] += 1;
                ++local.tasks;
                local.rows += std::min(block_rows, in.height - 2 - task * block_rows);
            }
        };
        if constexpr (method == Method::Static) {
            const int begin = total * id / team;
            const int end = total * (id + 1) / team;
            for (int task = begin; task < end; ++task) process(task);
        } else {
            while (true) {
                int task;
                if constexpr (method == Method::Lock) {
                    // 锁只保护编号领取；耗时的像素运算在锁外执行。
                    omp_set_lock(&lock);
                    task = next++;
                    omp_unset_lock(&lock);
                } else {
                    #pragma omp atomic capture
                    task = next++;
                }
                if constexpr (diagnose) ++local.claims;
                if (task >= total) break;
                process(task);
            }
        }
        if constexpr (diagnose) {
            local.elapsed_ms = (omp_get_wtime() - start) * 1000;
            stats->threads[id] = local;
        }
        // 并行区域结束时隐式同步，调用者不会读到尚未完成的输出。
    }
    if constexpr (method == Method::Lock) omp_destroy_lock(&lock);
}

template<bool diagnose>
void dispatch(const Image& in, Image& out, int threads, Method method,
              int block_rows, Diagnostics* stats) {
    switch (method) {
        case Method::Static:
            execute<Method::Static, diagnose>(in, out, threads, block_rows, stats); break;
        case Method::Lock:
            execute<Method::Lock, diagnose>(in, out, threads, block_rows, stats); break;
        case Method::Atomic:
            execute<Method::Atomic, diagnose>(in, out, threads, block_rows, stats); break;
        default: throw std::runtime_error("unknown method");
    }
}
}

void sobel_parallel(const Image& in, Image& out, int threads, Method method, int block_rows) {
    dispatch<false>(in, out, threads, method, block_rows, nullptr);
}

Diagnostics diagnose_parallel(const Image& in, Image& out, int threads, Method method, int block_rows) {
    if (threads < 1) throw std::runtime_error("threads must be positive");
    Diagnostics stats;
    stats.threads.resize(threads);
    stats.visits.resize(task_count(in, block_rows));
    dispatch<true>(in, out, threads, method, block_rows, &stats);
    return stats;
}

#ifndef SOBEL_LIBRARY
#include <iostream>
#include <iomanip>
#include <string>
int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::runtime_error("usage: parallel input.pgm output.pgm threads static|lock|atomic");
        const std::string name = argv[4];
        Method method;
        if (name == "static") method = Method::Static;
        else if (name == "lock") method = Method::Lock;
        else if (name == "atomic") method = Method::Atomic;
        else throw std::runtime_error("unknown method");
        omp_set_dynamic(0);
        auto in = read_pgm(argv[1]); Image out(in.width, in.height);
        const int threads = std::stoi(argv[3]);
        const double start = omp_get_wtime();
        sobel_parallel(in, out, threads, method);
        const double ms = (omp_get_wtime() - start) * 1000;
        write_pgm(argv[2], out);
        std::cout << std::setprecision(10) << "core_ms=" << ms << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
#endif
