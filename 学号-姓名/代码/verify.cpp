#include "sobel.hpp"
#include <omp.h>
#include <iostream>
#include <random>
// 参考实现直接遍历卷积核，不复用待测程序的单行函数。
Image oracle(const Image& a) {
  Image b(a.width,a.height);
  int kx[3][3]={{-1,0,1},{-2,0,2},{-1,0,1}};
  int ky[3][3]={{-1,-2,-1},{0,0,0},{1,2,1}};
  for(int y=1;y<a.height-1;++y) for(int x=1;x<a.width-1;++x) {
    int gx=0,gy=0;
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
      int v=a.pixels[size_t(y+dy)*a.width+x+dx];
      gx+=kx[dy+1][dx+1]*v; gy+=ky[dy+1][dx+1]*v;
    }
    b.pixels[size_t(y)*a.width+x]=uint8_t(std::min(255.0,std::sqrt(double(gx*gx+gy*gy))));
  }
  return b;
}
int main() {
    omp_set_dynamic(0);
    std::mt19937 rng(42);
    int comparisons = 0, ownership_checks = 0;
    for (auto wh : {std::pair<int,int>{1,1}, {2,7}, {7,2}, {3,3}, {5,5},
                    {19,13}, {64,37}, {17,66}, {17,67}, {23,131}, {31,257}}) {
        for (int kind = 0; kind < 5; ++kind) {
            Image in(wh.first, wh.second), out(wh.first, wh.second);
            for (int y = 0; y < in.height; ++y) for (int x = 0; x < in.width; ++x)
                in.pixels[size_t(y)*in.width+x] = kind == 0 ? 0 : kind == 1 ? 123 :
                    kind == 2 ? x*3 : kind == 3 ? (x >= in.width/2 ? 255 : 0) : rng()%256;
            const auto expected = oracle(in);
            sobel_serial(in, out);
            if (out.pixels != expected.pixels) return 1;
            ++comparisons;
            for (Method m : {Method::Static, Method::Lock, Method::Atomic})
                for (int threads : {1,2,3,4}) for (int block : {1,3,16,64}) {
                    std::fill(out.pixels.begin(), out.pixels.end(), 199);
                    sobel_parallel(in, out, threads, m, block);
                    if (out.pixels != expected.pixels) return 1;
                    ++comparisons;
                    const auto stats = diagnose_parallel(in, out, threads, m, block);
                    if (out.pixels != expected.pixels || stats.actual_threads != threads) return 1;
                    for (int visits : stats.visits) if (visits != 1) return 1;
                    int tasks = 0, rows = 0, claims = 0;
                    for (const auto& t : stats.threads) { tasks += t.tasks; rows += t.rows; claims += t.claims; }
                    if (tasks != task_count(in, block) || rows != std::max(0,in.height-2)) return 1;
                    if (claims != (m == Method::Static ? 0 : tasks + threads)) return 1;
                    ++ownership_checks;
                }
        }
    }
    Image ramp(5,5), out(5,5);
    for (int y=0; y<5; ++y) for (int x=0; x<5; ++x) ramp.pixels[y*5+x]=3*x;
    sobel_serial(ramp,out);
    if (out.pixels[12] != 24) return 1; // Gx=8×3，Gy=0。
    int rejected = 0;
    try { sobel_parallel(ramp,out,0,Method::Lock); } catch(const std::runtime_error&) { ++rejected; }
    try { sobel_parallel(ramp,out,2,Method::Atomic,0); } catch(const std::runtime_error&) { ++rejected; }
    try { sobel_parallel(ramp,ramp,2,Method::Static); } catch(const std::runtime_error&) { ++rejected; }
    if (rejected != 3) return 1;
    std::cout << comparisons << " oracle comparisons, " << ownership_checks
              << " diagnostic ownership checks, hand-calculated output=24, 3 invalid arguments passed\n";
}
