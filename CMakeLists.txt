cmake_minimum_required(VERSION 3.16)
project(cvm++ VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

set(SOURCES
    src/main.cpp
    src/value.cpp
    src/chunk.cpp
    src/lexer.cpp
    src/parser.cpp
    src/compiler.cpp
    src/disassembler.cpp
    src/ast_printer.cpp
    src/vm.cpp
    src/optimizer.cpp
    src/serializer.cpp
    src/repl.cpp
)

add_executable(cvm ${SOURCES})
target_include_directories(cvm PRIVATE src)

install(TARGETS cvm DESTINATION bin)
