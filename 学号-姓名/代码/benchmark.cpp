#include "sobel.hpp"
#include <omp.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <random>
#include <string>

struct Config { const char* name; int threads; Method method; };
int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("usage: benchmark input.pgm raw.csv output_directory");
        omp_set_dynamic(0);
        for (int p : {1, 2, 4}) {
            int actual = 0;
            #pragma omp parallel num_threads(p) shared(actual)
            {
                #pragma omp single
                actual = omp_get_num_threads();
            }
            if (actual != p) throw std::runtime_error("team size unavailable");
            std::cerr << "verified team size=" << actual << '\n';
        }
        const std::string dir = argv[3];
        auto in = read_pgm(argv[1]);
        Image reference(in.width, in.height), out(in.width, in.height);
        sobel_serial(in, reference);
        write_pgm((dir + "/serial.pgm").c_str(), reference);
        std::vector<Config> configs = {{"serial", 1, Method::Static}};
        for (auto m : {Method::Static, Method::Lock, Method::Atomic}) {
            const char* name = m == Method::Static ? "static" : m == Method::Lock ? "lock" : "atomic";
            for (int p : {1, 2, 4}) configs.push_back({name, p, m});
        }
        auto invoke = [&](const Config& c) {
            if (std::string(c.name) == "serial") sobel_serial(in, out);
            else sobel_parallel(in, out, c.threads, c.method);
        };
        for (const auto& c : configs) {
            for (int k = 0; k < 3; ++k) invoke(c);
            if (out.pixels != reference.pixels) throw std::runtime_error("warmup mismatch");
            if (std::string(c.name) != "serial")
                write_pgm((dir + "/" + c.name + "_" + std::to_string(c.threads) + ".pgm").c_str(), out);
        }
        std::ofstream csv(argv[2]);
        csv << "round,order,mode,threads,block_rows,core_ms,mismatch_pixels\n" << std::setprecision(12);
        std::ofstream quota(dir + "/配额计数.txt");
        auto snapshot = [&]() {
            std::ifstream f("/sys/fs/cgroup/cpu.stat");
            quota << f.rdbuf() << "---\n";
        };
        snapshot();
        std::mt19937 rng(20261025);
        for (int round = 1; round <= 30; ++round) {
            std::shuffle(configs.begin(), configs.end(), rng);
            int order = 0;
            for (const auto& c : configs) {
                const double start = omp_get_wtime();
                invoke(c);
                const double ms = (omp_get_wtime() - start) * 1000;
                size_t mismatch = 0;
                for (size_t i = 0; i < out.pixels.size(); ++i)
                    mismatch += out.pixels[i] != reference.pixels[i];
                csv << round << ',' << ++order << ',' << c.name << ',' << c.threads
                    << ",64," << ms << ',' << mismatch << '\n';
                if (mismatch) throw std::runtime_error("timed output mismatch");
            }
        }
        snapshot();
        csv.close();
        if (!csv) throw std::runtime_error("CSV write failed");
        std::cerr << "300 timed outputs matched serial exactly\n";

        // 正式测量结束后再采集诊断数据，不把计数与时钟调用混入计时。
        std::ofstream diag(dir + "/线程诊断数据.csv");
        diag << "run,mode,threads,thread_id,actual_threads,tasks,rows,claims,thread_elapsed_ms\n" << std::setprecision(12);
        for (int repeat = 1; repeat <= 3; ++repeat) {
            for (const auto& c : configs) {
                if (std::string(c.name) == "serial") continue;
                auto stats = diagnose_parallel(in, out, c.threads, c.method);
                if (stats.actual_threads != c.threads) throw std::runtime_error("team size mismatch");
                if (out.pixels != reference.pixels) throw std::runtime_error("diagnostic output mismatch");
                for (int visits : stats.visits)
                    if (visits != 1) throw std::runtime_error("missing or duplicate task");
                for (int id = 0; id < stats.actual_threads; ++id) {
                    const auto& t = stats.threads[id];
                    diag << repeat << ',' << c.name << ',' << c.threads << ',' << id << ','
                         << stats.actual_threads << ',' << t.tasks << ',' << t.rows << ','
                         << t.claims << ',' << t.elapsed_ms << '\n';
                }
            }
        }
        if (!diag) throw std::runtime_error("diagnostic CSV write failed");
        std::cerr << "27 diagnostic calls: every task visited exactly once; teams and pixels matched\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
