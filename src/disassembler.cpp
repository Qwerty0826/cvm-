#include "disassembler.h"
#include "common.h"
#include <iomanip>

static int simple_instruction(const char* name, int offset, std::ostream& out) {
    out << name << "\n";
    return offset + 1;
}

static int byte_instruction(const char* name, const Chunk& chunk, int offset, std::ostream& out) {
    uint8_t slot = chunk.code[offset + 1];
    out << std::left << std::setw(20) << name << " " << (int)slot << "\n";
    return offset + 2;
}

static int constant_instruction(const char* name, const Chunk& chunk, int offset, std::ostream& out) {
    int idx = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
    out << std::left << std::setw(20) << name
        << " " << std::setw(5) << idx
        << "  ; " << chunk.constants[idx] << "\n";
    return offset + 3;
}

static int jump_instruction(const char* name, int sign, const Chunk& chunk, int offset, std::ostream& out) {
    int jump = (chunk.code[offset + 1] << 8) | chunk.code[offset + 2];
    int target = offset + 3 + sign * jump;
    out << std::left << std::setw(20) << name
        << " 0x" << std::hex << std::setfill('0') << std::setw(4) << target
        << std::dec << std::setfill(' ') << "\n";
    return offset + 3;
}

int Disassembler::disassemble_instruction(const Chunk& chunk, int offset, std::ostream& out) {
    // Print byte offset + source line
    out << std::setfill('0') << std::setw(4) << std::hex << offset
        << std::dec << std::setfill(' ') << "  ";

    if (offset > 0 && chunk.lines[offset] == chunk.lines[offset - 1])
        out << "   | ";
    else
        out << std::setw(4) << chunk.lines[offset] << " ";

    out << " ";

    auto op = static_cast<OpCode>(chunk.code[offset]);
    switch (op) {
        // Simple (no operands)
        case OpCode::NIL:          return simple_instruction("NIL",          offset, out);
        case OpCode::TRUE:         return simple_instruction("TRUE",         offset, out);
        case OpCode::FALSE:        return simple_instruction("FALSE",        offset, out);
        case OpCode::POP:          return simple_instruction("POP",          offset, out);
        case OpCode::DUP:          return simple_instruction("DUP",          offset, out);
        case OpCode::ADD:          return simple_instruction("ADD",          offset, out);
        case OpCode::SUBTRACT:     return simple_instruction("SUBTRACT",     offset, out);
        case OpCode::MULTIPLY:     return simple_instruction("MULTIPLY",     offset, out);
        case OpCode::DIVIDE:       return simple_instruction("DIVIDE",       offset, out);
        case OpCode::MODULO:       return simple_instruction("MODULO",       offset, out);
        case OpCode::NEGATE:       return simple_instruction("NEGATE",       offset, out);
        case OpCode::NOT:          return simple_instruction("NOT",          offset, out);
        case OpCode::EQUAL:        return simple_instruction("EQUAL",        offset, out);
        case OpCode::NOT_EQUAL:    return simple_instruction("NOT_EQUAL",    offset, out);
        case OpCode::LESS:         return simple_instruction("LESS",         offset, out);
        case OpCode::LESS_EQUAL:   return simple_instruction("LESS_EQUAL",   offset, out);
        case OpCode::GREATER:      return simple_instruction("GREATER",      offset, out);
        case OpCode::GREATER_EQUAL:return simple_instruction("GREATER_EQUAL",offset, out);
        case OpCode::RETURN:       return simple_instruction("RETURN",       offset, out);
        case OpCode::PRINT:        return simple_instruction("PRINT",        offset, out);
        case OpCode::HALT:         return simple_instruction("HALT",         offset, out);

        // Byte operand
        case OpCode::GET_LOCAL:    return byte_instruction("GET_LOCAL",    chunk, offset, out);
        case OpCode::SET_LOCAL:    return byte_instruction("SET_LOCAL",    chunk, offset, out);
        case OpCode::CALL:         return byte_instruction("CALL",         chunk, offset, out);

        // 16-bit constant index
        case OpCode::CONSTANT:      return constant_instruction("CONSTANT",      chunk, offset, out);
        case OpCode::GET_GLOBAL:    return constant_instruction("GET_GLOBAL",    chunk, offset, out);
        case OpCode::DEFINE_GLOBAL: return constant_instruction("DEFINE_GLOBAL", chunk, offset, out);
        case OpCode::SET_GLOBAL:    return constant_instruction("SET_GLOBAL",    chunk, offset, out);
        case OpCode::INPUT:         return constant_instruction("INPUT",         chunk, offset, out);

        // Jumps
        case OpCode::JUMP:          return jump_instruction("JUMP",          +1, chunk, offset, out);
        case OpCode::JUMP_IF_FALSE: return jump_instruction("JUMP_IF_FALSE", +1, chunk, offset, out);
        case OpCode::JUMP_IF_TRUE:  return jump_instruction("JUMP_IF_TRUE",  +1, chunk, offset, out);
        case OpCode::LOOP:          return jump_instruction("LOOP",          -1, chunk, offset, out);

        default:
            out << "UNKNOWN(" << (int)chunk.code[offset] << ")\n";
            return offset + 1;
    }
}

void Disassembler::disassemble(const Chunk& chunk, std::ostream& out) {
    out << Color::CYAN() << "══ " << chunk.name << " ══" << Color::RESET() << "\n";
    out << Color::BOLD() << "OFFSET LINE  INSTRUCTION         OPERAND\n" << Color::RESET();
    out << std::string(50, '-') << "\n";
    int offset = 0;
    while (offset < (int)chunk.code.size()) {
        offset = disassemble_instruction(chunk, offset, out);
    }
    out << "\n";

    // Recursively disassemble nested function chunks stored in constants.
    for (auto& c : chunk.constants) {
        if (c.is_function() && c.fn) {
            disassemble(*c.fn->chunk, out);
        }
    }
}
