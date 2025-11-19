
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <cctype>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <stdint.h>

namespace ISA {
    enum Opcode : uint16_t {
        NOP   = 0x00,
        MOV   = 0x01,
        ADD   = 0x02,
        SUB   = 0x03,
        AND   = 0x04,
        OR    = 0x05,
        XOR   = 0x06,
        NOT_  = 0x07,
        SHL   = 0x08,
        SHR   = 0x09,
        CMP   = 0x0A,
        PUSH  = 0x0B,
        POP   = 0x0C,
        LD_ABS= 0x0D,
        ST_ABS= 0x0E,
        LDI   = 0x0F,
        JMP   = 0x10,
        JZ    = 0x11,
        JNZ   = 0x12,
        JC    = 0x13,
        JN    = 0x14,
        CALL  = 0x15,
        RET   = 0x16,
        HALT  = 0x17,
        LD_IND= 0x18,
        ST_IND= 0x19,
        LEA   = 0x1A,
        ADDI  = 0x1B,
        SUBI  = 0x1C,
        MUL   = 0x1D,
    };
}

static inline std::string trim(const std::string& s){
    size_t i=0,j=s.size();
    while(i<j && std::isspace((unsigned char)s[i])) i++;
    while(j>i && std::isspace((unsigned char)s[j-1])) j--;
    return s.substr(i,j-i);
}

static inline std::vector<std::string> tokenize(const std::string& line){
    std::vector<std::string> out;
    std::string t;
    bool in_str=false;
    for(size_t i=0;i<line.size();++i){
        char c=line[i];
        if(c=='"'){ t.push_back(c); in_str = !in_str; continue; }
        if(in_str){ t.push_back(c); continue; }
        if(c==',' || std::isspace((unsigned char)c)){
            if(!t.empty()){ out.push_back(t); t.clear(); }
        } else {
            t.push_back(c);
        }
    }
    if(!t.empty()) out.push_back(t);
    return out;
}

static inline bool is_register(const std::string& tok){
    if(tok=="sp") return true;
    if(tok.size()<2 || (tok[0]!='r' && tok[0]!='R')) return false;
    for(size_t i=1;i<tok.size();++i) if(!isdigit((unsigned char)tok[i])) return false;
    int v = std::stoi(tok.substr(1));
    return v>=0 && v<=7;
}

static inline uint16_t reg_id(const std::string& tok){
    if(tok=="sp") return 7;
    return (uint16_t)std::stoi(tok.substr(1));
}

static inline uint16_t encode_R(uint16_t opc, uint16_t rd, uint16_t rs){
    return (uint16_t)((opc<<11) | ((rd&7)<<8) | ((rs&7)<<5));
}

class Assembler {
public:
    std::vector<uint16_t> assemble_file(const std::string& path){
        std::ifstream f(path);
        if(!f) throw std::runtime_error("Cannot open " + path);
        std::string line;
        std::vector<std::string> lines;
        while(std::getline(f,line)){
            size_t p = line.find(';'); if(p!=std::string::npos) line=line.substr(0,p);
            p = line.find('#'); if(p!=std::string::npos) line=line.substr(0,p);
            lines.push_back(line);
        }
        first_pass(lines);
        return second_pass(lines);
    }

private:
    std::unordered_map<std::string,uint16_t> sym;
    uint16_t loc=0;

    void first_pass(const std::vector<std::string>& lines){
        sym.clear(); loc = 0;
        for(auto raw: lines){
            std::string line = trim(raw);
            if(line.empty()) continue;
            if(line.back()==':'){ std::string label = line.substr(0,line.size()-1); sym[label] = loc; continue; }
            if(line.rfind(".org",0)==0){
                auto toks = tokenize(line);
                if(toks.size()!=2) throw std::runtime_error(".org requires address");
                loc = parse_imm(toks[1]); continue;
            }
            if(line.rfind(".word",0)==0){
                auto p = line.find(' ');
                std::string rest = (p==std::string::npos)?"":line.substr(p+1);
                auto toks = tokenize(rest);
                if(toks.empty()) throw std::runtime_error(".word requires values");
                loc += (uint16_t)toks.size(); continue;
            }
            if(line.rfind(".asciiz",0)==0){
                auto p = line.find('"');
                auto q = line.rfind('"');
                if(p==std::string::npos || q==std::string::npos || q<p) throw std::runtime_error(".asciiz requires string literal");
                std::string s = line.substr(p+1, q-p-1);
                loc += (uint16_t)(s.size()+1); continue;
            }
            auto toks = tokenize(line);
            if(toks.empty()) continue;
            std::string op = upper(toks[0]);
            if(is_two_word(op, toks)) loc += 2;
            else loc += 1;
        }
    }

    std::vector<uint16_t> second_pass(const std::vector<std::string>& lines){
        std::vector<uint16_t> out;
        loc = 0;
        for(auto raw: lines){
            std::string line = trim(raw);
            if(line.empty()) continue;
            if(line.back()==':'){ continue; }
            if(line.rfind(".org",0)==0){
                auto toks = tokenize(line);
                loc = parse_imm(toks[1]);
                while(out.size() < loc) out.push_back(0);
                continue;
            }
            if(line.rfind(".word",0)==0){
                auto p = line.find(' ');
                std::string rest = (p==std::string::npos)?"":line.substr(p+1);
                auto toks = tokenize(rest);
                for(auto& t: toks){ uint16_t v = parse_imm_or_label(t); ensure_size(out, loc); out[loc++] = v; }
                continue;
            }
            if(line.rfind(".asciiz",0)==0){
                auto p = line.find('"');
                auto q = line.rfind('"');
                std::string s = line.substr(p+1, q-p-1);
                for(unsigned char c: s){ ensure_size(out, loc); out[loc++] = (uint16_t)c; }
                ensure_size(out, loc); out[loc++] = 0; continue;
            }
            auto toks = tokenize(line);
            if(toks.empty()) continue;
            std::string op = upper(toks[0]);

            auto putR = [&](uint16_t opc, uint16_t rd, uint16_t rs){ ensure_size(out, loc); out[loc++] = encode_R(opc, rd, rs); };
            auto putI = [&](uint16_t opc, uint16_t rd, uint16_t imm){ ensure_size(out, loc); out[loc++] = (uint16_t)((opc<<11) | ((rd&7)<<8)); ensure_size(out, loc); out[loc++] = imm; };
            auto putJ = [&](uint16_t opc, uint16_t addr){ ensure_size(out, loc); out[loc++] = (uint16_t)(opc<<11); ensure_size(out, loc); out[loc++] = addr; };

            if(op=="NOP"){ putR(ISA::NOP,0,0); continue; }
            if(op=="HALT"){ putR(ISA::HALT,0,0); continue; }
            if(op=="RET"){ putR(ISA::RET,0,0); continue; }

            if(op=="PUSH"){ if(toks.size()!=2 || !is_register(toks[1])) throw std::runtime_error("PUSH rs"); putR(ISA::PUSH,0,reg_id(toks[1])); continue; }
            if(op=="POP"){ if(toks.size()!=2 || !is_register(toks[1])) throw std::runtime_error("POP rd"); putR(ISA::POP,reg_id(toks[1]),0); continue; }

            if(op=="MOV"||op=="ADD"||op=="SUB"||op=="AND"||op=="OR"||op=="XOR"||op=="SHL"||op=="SHR"||op=="CMP"||op=="MUL"||op=="NOT"){
                if(op=="NOT"){
                    if(toks.size()!=2 || !is_register(toks[1])) throw std::runtime_error("NOT rd");
                    putR(ISA::NOT_, reg_id(toks[1]), 0); continue;
                }
                if(toks.size()!=3 || !is_register(toks[1]) || !is_register(toks[2])) throw std::runtime_error(op+" rd, rs");
                uint16_t rd = reg_id(toks[1]); uint16_t rs = reg_id(toks[2]);
                uint16_t opc = (op=="MOV")?ISA::MOV:(op=="ADD")?ISA::ADD:(op=="SUB")?ISA::SUB:(op=="AND")?ISA::AND:(op=="OR")?ISA::OR:(op=="XOR")?ISA::XOR:(op=="SHL")?ISA::SHL:(op=="SHR")?ISA::SHR:(op=="CMP")?ISA::CMP:(op=="MUL")?ISA::MUL:0;
                putR(opc, rd, rs); continue;
            }

            if(op=="LDI"||op=="LEA"||op=="ADDI"||op=="SUBI"){
                if(toks.size()!=3 || !is_register(toks[1])) throw std::runtime_error(op+" rd, imm16");
                putI(op=="LDI"?ISA::LDI: (op=="LEA"?ISA::LEA: (op=="ADDI"?ISA::ADDI:ISA::SUBI)), reg_id(toks[1]), parse_imm_or_label(toks[2])); continue;
            }

            if(op=="LD"){
                if(toks.size()!=3) throw std::runtime_error("LD rd, [addr] or LD rd, [rs]");
                if(!is_register(toks[1])) throw std::runtime_error("LD rd, [...]");
                std::string m = toks[2];
                if(m.size()<3 || m.front()!='[' || m.back()!=']') throw std::runtime_error("LD needs [..]");
                std::string inside = m.substr(1, m.size()-2);
                if(is_register(inside)){
                    uint16_t rd = reg_id(toks[1]); uint16_t rs = reg_id(inside);
                    ensure_size(out, loc); out[loc++] = encode_R(ISA::LD_IND, rd, rs);
                }else{
                    uint16_t addr = parse_imm_or_label(inside); uint16_t rd = reg_id(toks[1]);
                    ensure_size(out, loc); out[loc++] = (uint16_t)((ISA::LD_ABS<<11) | ((rd&7)<<8));
                    ensure_size(out, loc); out[loc++] = addr;
                }
                continue;
            }
            if(op=="ST"){
                if(toks.size()!=3) throw std::runtime_error("ST rs, [addr] or ST rs, [rd]");
                if(!is_register(toks[1])) throw std::runtime_error("ST rs, [...]");
                std::string m = toks[2];
                if(m.size()<3 || m.front()!='[' || m.back()!=']') throw std::runtime_error("ST needs [..]");
                std::string inside = m.substr(1, m.size()-2);
                if(is_register(inside)){
                    uint16_t rs = reg_id(toks[1]); uint16_t rd = reg_id(inside);
                    ensure_size(out, loc); out[loc++] = encode_R(ISA::ST_IND, rd, rs);
                }else{
                    uint16_t addr = parse_imm_or_label(inside); uint16_t rs = reg_id(toks[1]);
                    ensure_size(out, loc); out[loc++] = (uint16_t)((ISA::ST_ABS<<11) | ((0&7)<<8) | ((rs&7)<<5));
                    ensure_size(out, loc); out[loc++] = addr;
                }
                continue;
            }

            if(op=="JMP"||op=="JZ"||op=="JNZ"||op=="JC"||op=="JN"||op=="CALL"){
                uint16_t opc = (op=="JMP")?ISA::JMP:(op=="JZ")?ISA::JZ:(op=="JNZ")?ISA::JNZ:(op=="JC")?ISA::JC:(op=="JN")?ISA::JN:ISA::CALL;
                if(toks.size()!=2) throw std::runtime_error(op+" addr/label");
                putJ(opc, parse_imm_or_label(toks[1])); continue;
            }

            throw std::runtime_error("Unknown op: " + op);
        }
        return out;
    }

    static inline void ensure_size(std::vector<uint16_t>& out, uint16_t at){ if(out.size() <= at) out.resize(at+1, 0); }
    static inline std::string upper(std::string s){ for(char& c: s) c = (char)std::toupper((unsigned char)c); return s; }

    uint16_t parse_imm_or_label(const std::string& t){
        if(is_label_name(t)){ auto it = sym.find(t); if(it==sym.end()) throw std::runtime_error("Undefined label: " + t); return it->second; }
        return parse_imm(t);
    }

    static inline bool is_label_name(const std::string& t){
        if(t.empty()) return false;
        if(std::isdigit((unsigned char)t[0])) return false;
        if(t[0]=='.') return false;
        for(char c: t){ if(!(std::isalnum((unsigned char)c) || c=='_' )) return false; }
        return true;
    }

    static inline uint16_t parse_imm(const std::string& t){
        std::string s = t;
        if(s.size()>=3 && s[0]=='0' && (s[1]=='x' || s[1]=='X')) return (uint16_t)std::stoul(s, nullptr, 16);
        if(s.size()==3 && s.front()=='\'' && s.back()=='\'') return (uint16_t)(unsigned char)s[1];
        return (uint16_t)std::stoul(s, nullptr, 10);
    }

    bool is_two_word(const std::string& op, const std::vector<std::string>& toks){
        std::string u = upper(op);
        if(u=="LDI"||u=="LEA"||u=="ADDI"||u=="SUBI") return true;
        if(u=="LD"||u=="ST"){
            if(toks.size()>=3){
                std::string m = toks[2];
                if(m.size()>=3 && m.front()=='[' && m.back()==']'){
                    std::string inside = m.substr(1, m.size()-2);
                    if(is_register(inside)) return false; // indirect is one-word
                }
            }
            return true; // default assume absolute two-word
        }
        if(u=="JMP"||u=="JZ"||u=="JNZ"||u=="JC"||u=="JN"||u=="CALL") return true;
        return false;
    }
};
