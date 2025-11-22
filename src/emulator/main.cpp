#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include "Emu16.cpp"

static void usage(const char* argv0){
    std::cerr << "Usage: " << argv0 << " [--trace] [--memdump <file>] <program.bin>\n";
}

int main(int argc, char** argv){
    bool trace = false;
    std::string path;
    std::string memdump;

    for(int i=1;i<argc;i++){
        std::string a = argv[i];
        if(a == "--trace") {
            trace = true;
        } else if(a == "--memdump" && i+1 < argc) {
            memdump = argv[++i];
        } else if(a.size() && a[0] == '-') {
            usage(argv[0]);
            return 1;
        } else {
            path = a;
        }
    }
    if(path.empty()){ usage(argv[0]); return 1; }

    // load binary (little-endian bytes making 16-bit words)
    std::ifstream f(path, std::ios::binary);
    if(!f){ std::cerr << "Failed to open " << path << "\n"; return 1; }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), {});
    std::vector<uint16_t> rom;
    for(size_t i=0;i<bytes.size();){
        uint16_t w = bytes[i];
        if(i+1 < bytes.size()) w |= (uint16_t(bytes[i+1])<<8);
        rom.push_back(w);
        i += 2;
    }

    Emu16 emu(trace);
    emu.load(rom, 0x0000);
    emu.reset();
    emu.run();

    // --- NEW: full-memory dump after program finishes ---
    if(!memdump.empty()){
        std::ofstream md(memdump);
        if(!md){
            std::cerr << "Failed to open memdump file: " << memdump << "\n";
            return 1;
        }
        // One line per word: "ADDR VALUE" (both 4-hex uppercase)
        md.setf(std::ios::hex, std::ios::basefield);
        md.setf(std::ios::uppercase);
        md.fill('0');
        for(uint32_t addr = 0; addr < emu.mem.mem.size(); ++addr){
            md << std::setw(4) << addr << " " << std::setw(4) << (emu.mem.mem[addr] & 0xFFFF) << "\n";
        }
    }
    return 0;
}
