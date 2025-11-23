#!/bin/sh

mkdir -p build
cmake -B build -S .
cmake --build build

cd build
./asm16 ../programs/hello.asm -o hello.bin
./asm16 ../programs/factorial.asm -o factorial.bin
./asm16 ../programs/fibonacci.asm -o fibonacci.bin
./asm16 ../programs/timer.asm -o timer.bin

echo "\nRunning hello.bin..."
./emu16 hello.bin --memdump mem_hello.txt

echo "\n\nRunning factorial.bin..."
./emu16 factorial.bin --memdump mem_factorial.txt

echo "\nRunning fibonacci.bin..."
./emu16 fibonacci.bin --memdump mem_fibonacci.txt

