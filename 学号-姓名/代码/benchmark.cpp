#include "sobel.hpp"
#include <omp.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <random>
int main(int argc,char** argv) {
  try {
    if(argc!=4) throw std::runtime_error("usage: benchmark input.pgm raw.csv output_directory");
    omp_set_dynamic(0);
    for(int p:{1,2,4}) {
      int actual=0;
      #pragma omp parallel num_threads(p) shared(actual)
      {
        #pragma omp single
        actual=omp_get_num_threads();
      }
      if(actual!=p) throw std::runtime_error("requested team size unavailable");
      std::cerr<<"verified team size="<<actual<<'\n';
    }
    auto in=read_pgm(argv[1]); Image ref(in.width,in.height),out(in.width,in.height);
    sobel_serial(in,ref);
    write_pgm((std::string(argv[3])+"/serial.pgm").c_str(),ref);
    for(int p:{1,2,4}) {
      for(int k=0;k<3;++k) sobel_parallel(in,out,p);
      if(out.pixels!=ref.pixels) throw std::runtime_error("warmup mismatch");
      write_pgm((std::string(argv[3])+"/parallel_"+std::to_string(p)+".pgm").c_str(),out);
    }
    for(int k=0;k<3;++k) sobel_serial(in,out);
    std::ofstream csv(argv[2]);
    csv<<"round,order,mode,threads,core_ms,mismatch_pixels\n"<<std::setprecision(12);
    std::mt19937 rng(20261025);
    std::vector<int> configs={0,1,2,4};
    for(int round=1;round<=30;++round) {
      std::shuffle(configs.begin(),configs.end(),rng);
      int order=0;
      for(int p:configs) {
        double t=omp_get_wtime();
        if(p==0) sobel_serial(in,out); else sobel_parallel(in,out,p);
        double ms=(omp_get_wtime()-t)*1000;
        size_t mismatch=0;
        for(size_t i=0;i<out.pixels.size();++i) mismatch+=(out.pixels[i]!=ref.pixels[i]);
        csv<<round<<','<<++order<<','<<(p==0?"serial":"parallel")<<','<<(p==0?1:p)<<','<<ms<<','<<mismatch<<'\n';
        if(mismatch) throw std::runtime_error("measured output mismatch");
      }
    }
    if(!csv) throw std::runtime_error("CSV write failed");
    std::cerr<<"120 measured invocations passed exact comparison\n";
  } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
