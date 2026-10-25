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
void sobel_parallel(const Image&, Image&, int);
// Each invocation writes a complete row, including the zero-valued border.
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
        // Nonnegative gradient: floor after saturation, no normalization.
        dst[x]=static_cast<uint8_t>(std::min(255.0,std::sqrt(double(gx*gx+gy*gy))));
    }
}
inline void check_output(const Image& a,const Image& b) {
    if(a.width!=b.width || a.height!=b.height || &a==&b)
        throw std::runtime_error("output must be distinct with matching dimensions");
}
