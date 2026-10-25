#include "sobel.hpp"
#include <fstream>
#include <string>
#include <cctype>
static std::string token(std::istream& f) {
    std::string s;
    while(f) {
        int c=f.peek();
        if(c=='#') { std::getline(f,s); continue; }
        if(c!=EOF && std::isspace(c)) { f.get(); continue; }
        break;
    }
    if(!(f>>s)) throw std::runtime_error("truncated PGM header");
    return s;
}
Image read_pgm(const char* path) {
    std::ifstream f(path,std::ios::binary);
    if(!f || token(f)!="P5") throw std::runtime_error("expected binary P5 PGM");
    int w=std::stoi(token(f)),h=std::stoi(token(f)),max=std::stoi(token(f));
    if(w<1||h<1||size_t(w)*h>(size_t(1)<<30)||max!=255)
        throw std::runtime_error("unsupported dimensions or max value");
    int sep=f.get();
    if(sep==EOF || !std::isspace(sep)) throw std::runtime_error("missing separator");
    if(sep=='\r' && f.peek()=='\n') f.get();
    Image image(w,h);
    if(!f.read(reinterpret_cast<char*>(image.pixels.data()),image.pixels.size()))
        throw std::runtime_error("truncated pixel data");
    return image;
}
void write_pgm(const char* path,const Image& image) {
    std::ofstream f(path,std::ios::binary);
    f<<"P5\n"<<image.width<<' '<<image.height<<"\n255\n";
    f.write(reinterpret_cast<const char*>(image.pixels.data()),image.pixels.size());
    if(!f) throw std::runtime_error("cannot write image");
}
