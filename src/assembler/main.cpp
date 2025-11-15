
#include <iostream>
#include <string>
#include "Assembler.cpp"

static void usage(const char* argv0){
    std::cerr << "Usage: " << argv0 << " <file.asm> -o <out.bin>\n";
}

int main(int argc, char** argv){
    std::string in, out="a.bin";
    for(int i=1;i<argc;i++){
        std::string a = argv[i];
        if(a == "-o" && i+1<argc){ out = argv[++i]; }
        else if(a.rfind("-",0)==0){ usage(argv[0]); return 1; }
        else in = a;
    }
    if(in.empty()){ usage(argv[0]); return 1; }

    Assembler as;
    std::vector<uint16_t> words;
    try {
        words = as.assemble_file(in);
    } catch(const std::exception& e){
        std::cerr << "Assembly failed: " << e.what() << "\n";
        return 1;
    }

    std::ofstream f(out, std::ios::binary);
    if(!f){ std::cerr << "Failed to open " << out << " for writing\n"; return 1; }
    for(uint16_t w: words){
        uint8_t lo = w & 0xFF;
        uint8_t hi = (w >> 8) & 0xFF;
        f.put((char)lo);
        f.put((char)hi);
    }
    std::cout << "Wrote " << words.size()*2 << " bytes to " << out << "\n";
    return 0;
}
