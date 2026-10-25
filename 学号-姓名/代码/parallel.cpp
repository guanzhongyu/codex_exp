#include "sobel.hpp"
#include <omp.h>
void sobel_parallel(const Image& in,Image& out,int threads) {
    check_output(in,out);
    if(threads<1) throw std::runtime_error("threads must be positive");
    const int h=in.height;
    #pragma omp parallel for default(none) shared(in,out) firstprivate(h) num_threads(threads) schedule(static)
    for(int y=0;y<h;++y) sobel_row(in,out,y);
}
#ifndef SOBEL_LIBRARY
#include <iostream>
#include <iomanip>
int main(int argc,char** argv) {
    try {
        if(argc!=4) throw std::runtime_error("usage: parallel input.pgm output.pgm threads");
        omp_set_dynamic(0);
        int threads=std::stoi(argv[3]);
        auto in=read_pgm(argv[1]); Image out(in.width,in.height);
        double start=omp_get_wtime(); sobel_parallel(in,out,threads);
        double ms=(omp_get_wtime()-start)*1000;
        write_pgm(argv[2],out);
        std::cout<<std::setprecision(10)<<"core_ms="<<ms<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
#endif
