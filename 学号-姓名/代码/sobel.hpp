#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <stdexcept>
struct Image {
    int width, height;
    std::vector<uint8_t> pixels;
    Image(int w, int h): width(w), height(h), pixels(size_t(w)*h, 0) {
        if (w < 1 || h < 1) throw std::runtime_error("invalid size");
    }
};
Image read_pgm(const char* path);
void write_pgm(const char* path, const Image& image);
void sobel_serial(const Image&, Image&);
enum class Method { Static, Lock, Atomic };
struct ThreadStats {
    int tasks = 0;
    int rows = 0;
    int claims = 0;
    double elapsed_ms = 0;
};
struct Diagnostics {
    std::vector<ThreadStats> threads;
    std::vector<int> visits;
    int actual_threads = 0;
};
void sobel_parallel(const Image&, Image&, int, Method, int block_rows = 64);
Diagnostics diagnose_parallel(const Image&, Image&, int, Method, int block_rows = 64);
inline int task_count(const Image& in, int block_rows) {
    if (block_rows < 1) throw std::runtime_error("block size must be positive");
    const int rows = std::max(0, in.height - 2);
    return rows / block_rows + (rows % block_rows != 0);
}
inline void sobel_row(const Image&, Image&, int);
inline void sobel_block(const Image& in, Image& out, int task, int block_rows) {
    const int begin = 1 + task * block_rows;
    const int end = std::min(in.height - 1, begin + block_rows);
    for (int y = begin; y < end; ++y) sobel_row(in, out, y);
}
inline void sobel_row(const Image& in, Image& out, int y);
// 每行完整写回，避免上一次运行的边界值残留。
inline void sobel_row(const Image& in, Image& out, int y) {
    const int w=in.width, h=in.height;
    auto* dst=out.pixels.data()+size_t(y)*w;
    if (y==0 || y==h-1 || w<3) { std::fill(dst,dst+w,0); return; }
    dst[0]=dst[w-1]=0;
    const auto* a=in.pixels.data()+size_t(y-1)*w;
    const auto* b=a+w;
    const auto* c=b+w;
    for (int x=1; x<w-1; ++x) {
        const int gx=-a[x-1]+a[x+1]-2*b[x-1]+2*b[x+1]-c[x-1]+c[x+1];
        const int gy=-a[x-1]-2*a[x]-a[x+1]+c[x-1]+2*c[x]+c[x+1];
        // 幅值先饱和，再转换为8位灰度。
        dst[x]=static_cast<uint8_t>(std::min(255.0,std::sqrt(double(gx*gx+gy*gy))));
    }
}
inline void check_output(const Image& a,const Image& b) {
    if(a.width!=b.width || a.height!=b.height || &a==&b)
        throw std::runtime_error("output must be distinct with matching dimensions");
}
