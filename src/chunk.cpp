#include "chunk.h"
#include <stdexcept>

// ── Opcode names for the disassembler ─────────────────────────────────────────
const char* opcode_name(OpCode op) {
    switch (op) {
        case OpCode::CONSTANT:       return "CONSTANT";
        case OpCode::NIL:            return "NIL";
        case OpCode::TRUE:           return "TRUE";
        case OpCode::FALSE:          return "FALSE";
        case OpCode::POP:            return "POP";
        case OpCode::DUP:            return "DUP";
        case OpCode::GET_LOCAL:      return "GET_LOCAL";
        case OpCode::SET_LOCAL:      return "SET_LOCAL";
        case OpCode::GET_GLOBAL:     return "GET_GLOBAL";
        case OpCode::DEFINE_GLOBAL:  return "DEFINE_GLOBAL";
        case OpCode::SET_GLOBAL:     return "SET_GLOBAL";
        case OpCode::ADD:            return "ADD";
        case OpCode::SUBTRACT:       return "SUBTRACT";
        case OpCode::MULTIPLY:       return "MULTIPLY";
        case OpCode::DIVIDE:         return "DIVIDE";
        case OpCode::MODULO:         return "MODULO";
        case OpCode::NEGATE:         return "NEGATE";
        case OpCode::NOT:            return "NOT";
        case OpCode::EQUAL:          return "EQUAL";
        case OpCode::NOT_EQUAL:      return "NOT_EQUAL";
        case OpCode::LESS:           return "LESS";
        case OpCode::LESS_EQUAL:     return "LESS_EQUAL";
        case OpCode::GREATER:        return "GREATER";
        case OpCode::GREATER_EQUAL:  return "GREATER_EQUAL";
        case OpCode::JUMP:           return "JUMP";
        case OpCode::JUMP_IF_FALSE:  return "JUMP_IF_FALSE";
        case OpCode::JUMP_IF_TRUE:   return "JUMP_IF_TRUE";
        case OpCode::LOOP:           return "LOOP";
        case OpCode::CALL:           return "CALL";
        case OpCode::RETURN:         return "RETURN";
        case OpCode::PRINT:          return "PRINT";
        case OpCode::INPUT:          return "INPUT";
        case OpCode::HALT:           return "HALT";
        default:                     return "UNKNOWN";
    }
}

void Chunk::write(uint8_t byte, int line) {
    code.push_back(byte);
    lines.push_back(line);
}

int Chunk::add_constant(Value v) {
    if (constants.size() >= MAX_CONSTANTS)
        throw std::overflow_error("Too many constants in one chunk.");
    constants.push_back(std::move(v));
    return static_cast<int>(constants.size()) - 1;
}

void Chunk::write_constant(Value v, int line) {
    int idx = add_constant(std::move(v));
    write_op(OpCode::CONSTANT, line);
    write(static_cast<uint8_t>((idx >> 8) & 0xFF), line);
    write(static_cast<uint8_t>(idx & 0xFF), line);
}

void Chunk::patch_jump(int offset) {
    // The two bytes at [offset] and [offset+1] hold a placeholder (0xFF 0xFF).
    // Replace with the real forward distance from (offset+2) to end of code.
    int jump = current_offset() - (offset + 2);
    if (jump > 0xFFFF)
        throw std::overflow_error("Jump target too far.");
    code[offset]     = static_cast<uint8_t>((jump >> 8) & 0xFF);
    code[offset + 1] = static_cast<uint8_t>(jump & 0xFF);
}
