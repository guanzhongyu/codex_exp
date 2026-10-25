#include "sobel.hpp"
#include <omp.h>
#include <iostream>
#include <random>
// Independent, straightforward 3x3 reference; does not call sobel_row.
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
  omp_set_dynamic(0); std::mt19937 rng(42); int count=0;
  for(auto wh: {std::pair<int,int>{1,1},{2,7},{7,2},{3,3},{5,5},{19,13},{64,37}}) {
    for(int kind=0;kind<5;++kind) {
      Image in(wh.first,wh.second),out(wh.first,wh.second);
      for(int y=0;y<in.height;++y) for(int x=0;x<in.width;++x)
        in.pixels[size_t(y)*in.width+x]=kind==0?0:kind==1?123:kind==2?x*3:kind==3?(x>=in.width/2?255:0):rng()%256;
      auto expected=oracle(in);
      for(int p: {0,1,2,4}) {
        std::fill(out.pixels.begin(),out.pixels.end(),199);
        if(p==0) sobel_serial(in,out); else sobel_parallel(in,out,p);
        if(out.pixels!=expected.pixels) { std::cerr<<"oracle mismatch\n"; return 1; }
        ++count;
      }
    }
  }
  Image ramp(5,5),out(5,5);
  for(int y=0;y<5;++y)for(int x=0;x<5;++x)ramp.pixels[y*5+x]=3*x;
  sobel_serial(ramp,out);
  if(out.pixels[12]!=24) return 1; // Gx=8*3, Gy=0.
  std::cout<<count<<" oracle comparisons + hand-calculated center=24 passed\n";
}
