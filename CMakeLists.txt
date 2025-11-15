
cmake_minimum_required(VERSION 3.15)
project(Custom16CPU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(emu16
    src/emulator/main.cpp
    src/emulator/Emu16.cpp
)

add_executable(asm16
    src/assembler/main.cpp
    src/assembler/Assembler.cpp
)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

install(TARGETS emu16 asm16 RUNTIME DESTINATION bin)
