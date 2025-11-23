
# Custom 16-bit CPU — ISA, Emulator, Assembler, Tests

This project contains a complete 16-bit CPU ISA and emulator:
- A compact **ISA** with recursion support via `CALL`/`RET` and a downward stack on `SP` (`r7`).
- A **C++17 emulator** with ALU, Control Unit, Bus, Memory, and **memory-mapped I/O** (MMIO).
- A minimal **two-pass assembler** with labels, numeric literals, and basic directives.
- 65KiB memory size.
- The program is loaded at 0x0000 starting address, which can be confirmed in the memory dump!
- Stack starts at 0xF000 address.
- **Test programs**: Hello World, Recursive Factorial, and a Timer demo.

## Build

```bash
mkdir -p build
cd build
cmake ..
cmake --build . --config Release
```

This produces two binaries:
- `emu16` — the emulator
- `asm16` — the assembler

## ISA Overview

- **Word size**: 16 bits. Memory: 64 KiB *words* (addressable by 16-bit addresses).
- **Registers**: 8 × 16-bit (`r0`..`r7`). `r7` is `SP` (stack pointer). `PC` is separate.
- **Stack**: grows downward. `PUSH` decrements `SP` then writes; `POP` reads then increments.
- **Flags**: `Z` (zero), `N` (negative), `C` (carry/borrow), `V` (signed overflow).
- **Instruction length**: 1 or 2 words. If an instruction needs an immediate/address, the *next* word is the payload.
- **Encoding** (high level):
  - `[15:11]` opcode (5 bits, up to 32 opcodes)
  - `[10:8]` destination register `rd` (3 bits)
  - `[7:5]` source register `rs` (3 bits)
  - Remaining bits reserved/unused (for simple formats)
  - For `*_IMM`/`*_ABS` forms, a second word follows with the 16-bit immediate/address.

### Opcodes (subset used in examples)
| Opcode | Mnemonic / Form                 | Size | Description |
|-------:|---------------------------------|-----:|-------------|
| 0x00   | `NOP`                           | 1    | No operation |
| 0x01   | `MOV  rd, rs`                   | 1    | `rd = rs` |
| 0x02   | `ADD  rd, rs`                   | 1    | `rd = rd + rs` |
| 0x03   | `SUB  rd, rs`                   | 1    | `rd = rd - rs` |
| 0x04   | `AND  rd, rs`                   | 1    | Bitwise and |
| 0x05   | `OR   rd, rs`                   | 1    | Bitwise or |
| 0x06   | `XOR  rd, rs`                   | 1    | Bitwise xor |
| 0x07   | `NOT  rd`                       | 1    | Bitwise not (`rs` ignored) |
| 0x08   | `SHL  rd, rs`                   | 1    | Logical left shift by `rs & 0xF` |
| 0x09   | `SHR  rd, rs`                   | 1    | Logical right shift by `rs & 0xF` |
| 0x0A   | `CMP  rd, rs`                   | 1    | Sets flags for `rd - rs` |
| 0x0B   | `PUSH rs`                       | 1    | `SP -= 1; [SP] = rs` |
| 0x0C   | `POP  rd`                       | 1    | `rd = [SP]; SP += 1` |
| 0x0D   | `LD   rd, [imm16]`              | 2    | Absolute load word |
| 0x0E   | `ST   rs, [imm16]`              | 2    | Absolute store word |
| 0x0F   | `LDI  rd, imm16`                | 2    | Load immediate |
| 0x10   | `JMP  imm16`                    | 2    | Absolute jump |
| 0x11   | `JZ   imm16`                    | 2    | Jump if `Z=1` |
| 0x12   | `JNZ  imm16`                    | 2    | Jump if `Z=0` |
| 0x13   | `JC   imm16`                    | 2    | Jump if `C=1` |
| 0x14   | `JN   imm16`                    | 2    | Jump if `N=1` |
| 0x15   | `CALL imm16`                    | 2    | Push return addr; `PC = imm16` |
| 0x16   | `RET`                           | 1    | `PC = POP()` |
| 0x17   | `HALT`                          | 1    | Stop emulator |
| 0x18   | `LD   rd, [rs]`                 | 1    | Indirect load from address in `rs` |
| 0x19   | `ST   rs, [rd]`                 | 1    | Indirect store to address in `rd` |
| 0x1A   | `LEA  rd, imm16`                | 2    | Load effective address (alias of `LDI`) |
| 0x1B   | `ADDI rd, imm16`                | 2    | `rd += imm16` |
| 0x1C   | `SUBI rd, imm16`                | 2    | `rd -= imm16` |
| 0x1D   | `MUL  rd, rs`                   | 1    | Low-16 result of `rd * rs`, flags set |

**Calling convention:** single-register return/argument in `r0`. `CALL/RET` plus `PUSH/POP` allow recursion.

## Memory-Mapped I/O

| Address | Behavior |
|---------|----------|
| `0xFF00` | Write: low 8 bits are printed as a character |
| `0xFF10` | Write: address of a zero-terminated string (bytes in low 8 bits of words) to print |
| `0xFF12` | Write: print unsigned 16-bit integer in decimal and newline |
| `0xFF20` | Read: free-running timer counter (`cycles & 0xFFFF`) |

## Assembler

Two-pass assembler with:
- Labels (`start:`), registers `r0..r7` (or `sp` for `r7`)
- Numeric literals: decimal (`123`), hex (`0xABCD`), char (`'A'`)
- Directives:
  - `.org <addr>`: set origin (word address)
  - `.word <val>[, ...]`: emit one or more 16-bit words
  - `.asciiz "text"`: emit bytes (one per word) terminated by zero

## Test Programs

hello.bin: Prints out "Hello, World!" without a newline at the end.<br>
factorial.bin: Prints the factorial of 5.<br>
fibonacci.bin: Prints the fibonacci sequence for input 1 to 10.<br>

## Usage

Assemble a program and run it:

```bash
# From project root:
mkdir -p build && cd build
cmake .. && cmake --build . -j

# Assemble
./asm16 ../programs/hello.asm -o hello.bin
./asm16 ../programs/factorial.asm -o factorial.bin
./asm16 ../programs/fibonacci.asm -o fibonacci.bin
./asm16 ../programs/timer.asm -o timer.bin

# Run (optionally with --trace to show Fetch/Execute/Write)
# --memdump mem.txt will output the memory to mem.txt file as HEX with address and value.
./emu16 hello.bin --memdump mem.txt
./emu16 factorial.bin --memdump mem.txt
./emu16 fibonacci.bin --memdump mem.txt
./emu16 --trace timer.bin # --trace command prints out execution information.
```

The timer demo shows the **Fetch / Execute / Write** trace lines to illustrate cycles.

