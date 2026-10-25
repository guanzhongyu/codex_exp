#include "sobel.hpp"
void sobel_serial(const Image& in,Image& out) {
    check_output(in,out);
    sobel_row(in, out, 0);
    if (in.height > 1) sobel_row(in, out, in.height - 1);
    for (int task = 0; task < task_count(in, 64); ++task)
        sobel_block(in, out, task, 64);
}
#ifndef SOBEL_LIBRARY
#include <iostream>
#include <iomanip>
#include <omp.h>
int main(int argc,char** argv) {
    try {
        if(argc!=3) throw std::runtime_error("usage: serial input.pgm output.pgm");
        auto in=read_pgm(argv[1]); Image out(in.width,in.height);
        double start=omp_get_wtime(); sobel_serial(in,out);
        double ms=(omp_get_wtime()-start)*1000;
        write_pgm(argv[2],out);
        std::cout<<std::setprecision(10)<<"core_ms="<<ms<<'\n';
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
#endif
