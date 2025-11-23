#pragma once

/**
 * 16-bit CPU Emulator Core (Emu16.cpp)
 * -----------------------------------------------------------------------------
 * This file implements a tiny educational 16‑bit CPU:
 *   • 8 general-purpose registers (r0..r7) where r7 is SP (stack pointer)
 *   • Separate PC and FLAGS (N,Z,C,V)
 *   • 64K x 16-bit word-addressable memory
 *   • Memory-mapped I/O (MMIO) at 0xFF00..0xFFFF:
 *       - 0xFF00: TX_CHAR (write low 8 bits -> prints a character)
 *       - 0xFF10: TX_STR_ADDR (write address of zero-terminated string)
 *       - 0xFF12: TX_INT (write 16-bit integer as decimal + newline)
 *       - 0xFF20: TIMER (read-only, returns cycles & 0xFFFF)
 *
 * Conventions
 *   • Stack grows downward. On reset, SP = 0xF000 (kept below MMIO window).
 *   • CALL pushes the return address, RET pops it back into PC.
 *   • All GPRs are caller-saved in sample programs.
 *
 * Reading guide
 *   1) ISA opcodes (enum) .............................................. [ISA]
 *   2) Flags struct and helpers ...................................... [Flags]
 *   3) MMIO implementation ............................................ [MMIO]
 *   4) Memory wrapper (RAM + MMIO) .................................. [Memory]
 *   5) ALU operations and flag logic ................................... [ALU]
 *   6) Emu16 CPU core: reset/load/fetch/execute loop .................. [Emu16]
 *
 * Tip: enable trace mode in the frontend to print per-instruction state.
 *      (frontend parses --trace and calls Emu16 with trace=true)
 *
 * This file is a formatting + documentation pass over your original.
 * Logic is preserved; only comments/whitespace were adjusted for clarity.
 */

#include <cstdint>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cassert>
#include <functional>
#include <stdint.h>

// [ISA] Instruction set opcodes used by the decoder and encoder
namespace ISA
{
    enum Opcode : uint16_t
    {
        NOP = 0x00,
        MOV = 0x01,
        ADD = 0x02,
        SUB = 0x03,
        AND = 0x04,
        OR = 0x05,
        XOR = 0x06,
        NOT_ = 0x07,
        SHL = 0x08,
        SHR = 0x09,
        CMP = 0x0A,
        PUSH = 0x0B,
        POP = 0x0C,
        LD_ABS = 0x0D,
        ST_ABS = 0x0E,
        LDI = 0x0F,
        JMP = 0x10,
        JZ = 0x11,
        JNZ = 0x12,
        JC = 0x13,
        JN = 0x14,
        CALL = 0x15,
        RET = 0x16,
        HALT = 0x17,
        LD_IND = 0x18,
        ST_IND = 0x19,
        LEA = 0x1A,
        ADDI = 0x1B,
        SUBI = 0x1C,
        MUL = 0x1D,
    };
}

// [Flags] Processor status flags: Negative, Zero, Carry, Overflow
struct Flags
{
    bool Z = false, N = false, C = false, V = false;
};


static inline std::string flags_str(const Flags &f)
{
    std::ostringstream oss;
    oss << (f.N ? 'N' : '-') << (f.Z ? 'Z' : '-') << (f.C ? 'C' : '-') << (f.V ? 'V' : '-');
    return oss.str();
}

// [MMIO] Minimal memory-mapped I/O devices per the map above
struct MMIO
{
    void write(uint16_t addr, uint16_t value)
    {
        switch (addr)
        {
        case 0xFF00:
        {
            char c = char(value & 0xFF);
            std::cout << c << std::flush;
        }
        break;
        case 0xFF10:
        {
            pending_string_addr = value;
            trigger_string_print = true;
        }
        break;
        case 0xFF12:
        {
            std::cout << (value) << "\n";
        }
        break;
        default:
            break;
        }
    }
    uint16_t read(uint16_t addr, uint64_t cycles, const std::function<uint16_t(uint16_t)> &mem_read)
    {
        if (addr == 0xFF20)
        {
            return static_cast<uint16_t>(cycles & 0xFFFF);
        }
        return 0;
    }
    void service_pending(const std::function<uint16_t(uint16_t)> &mem_read)
    {
        if (trigger_string_print)
        {
            uint16_t a = pending_string_addr;
            while (true)
            {
                uint16_t w = mem_read(a++);
                uint8_t b = uint8_t(w & 0xFF);
                if (b == 0)
                    break;
                std::cout << char(b);
            }
            std::cout.flush();
            trigger_string_print = false;
        }
    }
    uint16_t pending_string_addr = 0;
    bool trigger_string_print = false;
};

// [Memory] 64K-word RAM plus MMIO window at 0xFF00..0xFFFF
struct Memory
{
    std::vector<uint16_t> mem;
    MMIO io;
    Memory() : mem(65536, 0) {}
    uint16_t read(uint16_t addr, uint64_t cycles)
    {
        if (addr >= 0xFF00)
            return io.read(addr, cycles, [&](uint16_t a)
                           { return mem[a]; });
        return mem[addr];
    }
    void write(uint16_t addr, uint16_t value)
    {
        if (addr >= 0xFF00)
        {
            io.write(addr, value);
            return;
        }
        mem[addr] = value;
    }
};

// [ALU] Arithmetic/logic unit with flag updates for ADD/SUB/logic/shift/mul
struct ALU
{
    static uint16_t add(uint16_t a, uint16_t b, Flags &f)
    {
        uint32_t r = uint32_t(a) + uint32_t(b);
        uint16_t res = uint16_t(r & 0xFFFF);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = (r >> 16) & 1;
        f.V = (~(a ^ b) & (res ^ a) & 0x8000) != 0;
        return res;
    }
    static uint16_t sub(uint16_t a, uint16_t b, Flags &f)
    {
        uint32_t r = uint32_t(a) - uint32_t(b);
        uint16_t res = uint16_t(r & 0xFFFF);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = (r >> 16) & 1;
        f.V = ((a ^ b) & (a ^ res) & 0x8000) != 0;
        return res;
    }
    static uint16_t band(uint16_t a, uint16_t b, Flags &f)
    {
        uint16_t res = (a & b);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = false;
        f.V = false;
        return res;
    }
    static uint16_t bor(uint16_t a, uint16_t b, Flags &f)
    {
        uint16_t res = (a | b);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = false;
        f.V = false;
        return res;
    }
    static uint16_t bxor(uint16_t a, uint16_t b, Flags &f)
    {
        uint16_t res = (a ^ b);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = false;
        f.V = false;
        return res;
    }
    static uint16_t bnot(uint16_t a, Flags &f)
    {
        uint16_t res = ~a;
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = false;
        f.V = false;
        return res;
    }
    static uint16_t shl(uint16_t a, uint16_t b, Flags &f)
    {
        uint8_t sh = uint8_t(b & 0xF);
        uint32_t r = uint32_t(a) << sh;
        uint16_t res = uint16_t(r & 0xFFFF);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = (sh == 0) ? f.C : ((a << (sh - 1)) & 0x8000);
        f.V = false;
        return res;
    }
    static uint16_t shr(uint16_t a, uint16_t b, Flags &f)
    {
        uint8_t sh = uint8_t(b & 0xF);
        uint16_t res = (sh >= 16) ? 0 : (a >> sh);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = (sh == 0) ? f.C : ((a >> (sh - 1)) & 1);
        f.V = false;
        return res;
    }
    static uint16_t mul(uint16_t a, uint16_t b, Flags &f)
    {
        uint32_t r = uint32_t(a) * uint32_t(b);
        uint16_t res = uint16_t(r & 0xFFFF);
        f.Z = (res == 0);
        f.N = (res & 0x8000) != 0;
        f.C = (r >> 16) != 0;
        f.V = false;
        return res;
    }
};


// [Emu16] CPU core: registers, PC/FLAGS, fetch/decode/execute loop
struct Emu16
{
    bool trace = false;
    Memory mem;

    // General purpose registers, where R7 is SP.
    uint16_t R[8] = {0};
    
    uint16_t PC = 0;
    Flags F{};
    bool halted = false;
    uint64_t cycles = 0;

    Emu16(bool trace_) : trace(trace_) {}

    void reset()
    {
        for (int i = 0; i < 8; i++)
            R[i] = 0;
        PC = 0;
        F = Flags{};
        halted = false;
        cycles = 0;
        R[7] = 0xF000; // SP below MMIO (0xFF00..0xFFFF)
    }
    void load(const std::vector<uint16_t> &image, uint16_t base)
    {
        for (size_t i = 0; i < image.size(); ++i)
        {
            uint16_t addr = base + uint16_t(i);
            if (addr < mem.mem.size())
                mem.mem[addr] = image[i];
        }
    }

    inline uint16_t fetch()
    {
        uint16_t w = mem.read(PC, cycles);
        if (trace)
        {
            std::cout << "[FETCH] PC=" << hex4(PC) << " W=" << hex4(w) << "\n";
        }
        PC++;
        cycles++;
        return w;
    }
    static inline std::string hex4(uint16_t x)
    {
        std::ostringstream oss;
        oss << "0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << x;
        return oss.str();
    }

    void run()
    {
        while (!halted)
        {
            uint16_t inst = fetch();
            uint16_t opcode = (inst >> 11) & 0x1F;
            uint16_t rd = (inst >> 8) & 0x7;
            uint16_t rs = (inst >> 5) & 0x7;
            switch (opcode)
            {
            case ISA::NOP:
                break;
            case ISA::MOV:
            {
                if (trace)
                    std::cout << "  [EXEC] MOV r" << rd << ", r" << rs << "\n";
                write_reg(rd, R[rs]);
            }
            break;
            case ISA::ADD:
            {
                if (trace)
                    std::cout << "  [EXEC] ADD r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::add(R[rd], R[rs], F));
            }
            break;
            case ISA::SUB:
            {
                if (trace)
                    std::cout << "  [EXEC] SUB r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::sub(R[rd], R[rs], F));
            }
            break;
            case ISA::AND:
            {
                if (trace)
                    std::cout << "  [EXEC] AND r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::band(R[rd], R[rs], F));
            }
            break;
            case ISA::OR:
            {
                if (trace)
                    std::cout << "  [EXEC] OR r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::bor(R[rd], R[rs], F));
            }
            break;
            case ISA::XOR:
            {
                if (trace)
                    std::cout << "  [EXEC] XOR r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::bxor(R[rd], R[rs], F));
            }
            break;
            case ISA::NOT_:
            {
                if (trace)
                    std::cout << "  [EXEC] NOT r" << rd << "\n";
                write_reg(rd, ALU::bnot(R[rd], F));
            }
            break;
            case ISA::SHL:
            {
                if (trace)
                    std::cout << "  [EXEC] SHL r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::shl(R[rd], R[rs], F));
            }
            break;
            case ISA::SHR:
            {
                if (trace)
                    std::cout << "  [EXEC] SHR r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::shr(R[rd], R[rs], F));
            }
            break;
            case ISA::CMP:
            {
                if (trace)
                    std::cout << "  [EXEC] CMP r" << rd << ", r" << rs << "\n";
                (void)ALU::sub(R[rd], R[rs], F);
            }
            break;
            case ISA::PUSH:
            {
                if (trace)
                    std::cout << "  [EXEC] PUSH r" << rs << "\n";
                R[7] -= 1;
                mem.write(R[7], R[rs]);
                cycles++;
                if (trace)
                    std::cout << "  [WRITE] [SP=" << hex4(R[7]) << "] = " << hex4(R[rs]) << "\n";
            }
            break;
            case ISA::POP:
            {
                if (trace)
                    std::cout << "  [EXEC] POP r" << rd << "\n";
                uint16_t v = mem.read(R[7], cycles);
                write_reg(rd, v);
                R[7] += 1;
                cycles++;
            }
            break;
            case ISA::LD_ABS:
            {
                uint16_t addr = fetch();
                uint16_t v = (addr >= 0xFF00) ? mem.io.read(addr, cycles, [&](uint16_t a)
                                                            { return mem.mem[a]; })
                                              : mem.read(addr, cycles);
                if (trace)
                    std::cout << "  [EXEC] LD r" << rd << ", [" << hex4(addr) << "] -> " << hex4(v) << "\n";
                write_reg(rd, v);
            }
            break;
            case ISA::ST_ABS:
            {
                uint16_t addr = fetch();
                if (trace)
                    std::cout << "  [EXEC] ST r" << rs << ", [" << hex4(addr) << "] = " << hex4(R[rs]) << "\n";
                mem.write(addr, R[rs]);
                cycles++;
            }
            break;
            case ISA::LDI:
            {
                uint16_t imm = fetch();
                if (trace)
                    std::cout << "  [EXEC] LDI r" << rd << ", " << hex4(imm) << "\n";
                write_reg(rd, imm);
            }
            break;
            case ISA::JMP:
            {
                uint16_t addr = fetch();
                if (trace)
                    std::cout << "  [EXEC] JMP " << hex4(addr) << "\n";
                PC = addr;
            }
            break;
            case ISA::JZ:
            {
                uint16_t addr = fetch();
                if (trace)
                    std::cout << "  [EXEC] JZ " << hex4(addr) << " (Z=" << F.Z << ")\n";
                if (F.Z)
                    PC = addr;
            }
            break;
            case ISA::JNZ:
            {
                uint16_t addr = fetch();
                if (trace)
                    std::cout << "  [EXEC] JNZ " << hex4(addr) << " (Z=" << F.Z << ")\n";
                if (!F.Z)
                    PC = addr;
            }
            break;
            case ISA::JC:
            {
                uint16_t addr = fetch();
                if (trace)
                    std::cout << "  [EXEC] JC " << hex4(addr) << " (C=" << F.C << ")\n";
                if (F.C)
                    PC = addr;
            }
            break;
            case ISA::JN:
            {
                uint16_t addr = fetch();
                if (trace)
                    std::cout << "  [EXEC] JN " << hex4(addr) << " (N=" << F.N << ")\n";
                if (F.N)
                    PC = addr;
            }
            break;
            case ISA::CALL:
            {
                uint16_t addr = fetch();
                if (trace)
                    std::cout << "  [EXEC] CALL " << hex4(addr) << " (push RA=" << hex4(PC) << ")\n";
                R[7] -= 1;
                mem.write(R[7], PC);
                PC = addr;
                cycles++;
            }
            break;
            case ISA::RET:
            {
                uint16_t ra = mem.read(R[7], cycles);
                R[7] += 1;
                if (trace)
                    std::cout << "  [EXEC] RET -> " << hex4(ra) << "\n";
                PC = ra;
                cycles++;
            }
            break;
            case ISA::HALT:
            {
                if (trace)
                    std::cout << "  [EXEC] HALT\n";
                halted = true;
            }
            break;
            case ISA::LD_IND:
            {
                uint16_t addr = R[rs];
                uint16_t v = mem.read(addr, cycles);
                if (trace)
                    std::cout << "  [EXEC] LD r" << rd << ", [r" << rs << "=" << hex4(addr) << "] -> " << hex4(v) << "\n";
                write_reg(rd, v);
            }
            break;
            case ISA::ST_IND:
            {
                uint16_t addr = R[rd];
                if (trace)
                    std::cout << "  [EXEC] ST r" << rs << " -> [r" << rd << "=" << hex4(addr) << "] = " << hex4(R[rs]) << "\n";
                mem.write(addr, R[rs]);
                cycles++;
            }
            break;
            case ISA::LEA:
            {
                uint16_t imm = fetch();
                if (trace)
                    std::cout << "  [EXEC] LEA r" << rd << ", " << hex4(imm) << "\n";
                write_reg(rd, imm);
            }
            break;
            case ISA::ADDI:
            {
                uint16_t imm = fetch();
                if (trace)
                    std::cout << "  [EXEC] ADDI r" << rd << ", " << hex4(imm) << "\n";
                write_reg(rd, ALU::add(R[rd], imm, F));
            }
            break;
            case ISA::SUBI:
            {
                uint16_t imm = fetch();
                if (trace)
                    std::cout << "  [EXEC] SUBI r" << rd << ", " << hex4(imm) << "\n";
                write_reg(rd, ALU::sub(R[rd], imm, F));
            }
            break;
            case ISA::MUL:
            {
                if (trace)
                    std::cout << "  [EXEC] MUL r" << rd << ", r" << rs << "\n";
                write_reg(rd, ALU::mul(R[rd], R[rs], F));
            }
            break;
            default:
            {
                std::cerr << "Unknown opcode: " << opcode << " at " << hex4(PC - 1) << "\n";
                halted = true;
            }
            break;
            }
            mem.io.service_pending([&](uint16_t a)
                                   { return mem.mem[a]; });
            if (trace)
            {
                std::cout << "  [STATE] PC=" << hex4(PC) << " SP=" << hex4(R[7])
                          << " R0=" << hex4(R[0]) << " R1=" << hex4(R[1])
                          << " FLAGS=" << flags_str(F) << " CYC=" << cycles << "\n";
            }
        }
    }

    void write_reg(uint16_t rd, uint16_t v)
    {
        R[rd] = v;
        F.Z = (v == 0);
        F.N = (v & 0x8000) != 0;
        if (trace)
        {
            std::cout << "  [WRITE] r" << rd << " = " << hex4(v) << " (FLAGS " << flags_str(F) << ")\n";
        }
        cycles++;
    }
};
