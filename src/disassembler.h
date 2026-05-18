#pragma once
#include "chunk.h"
#include <ostream>

// Prints human-readable bytecode for debugging / --dump-bytecode mode.
class Disassembler {
public:
    // Disassemble an entire chunk to the given stream.
    static void disassemble(const Chunk& chunk, std::ostream& out);

    // Disassemble a single instruction at byte offset.
    // Returns the offset of the NEXT instruction.
    static int disassemble_instruction(const Chunk& chunk, int offset, std::ostream& out);
};
