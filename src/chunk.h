#pragma once
#include "common.h"
#include "value.h"

// ── Instruction Set Architecture ──────────────────────────────────────────────
enum class OpCode : uint8_t {
    // ── Constants ────────────────────────────────────────────────────
    CONSTANT,        // [hi, lo]  push constants[hi<<8|lo]
    NIL,             // push nil
    TRUE,            // push true
    FALSE,           // push false

    // ── Stack ops ────────────────────────────────────────────────────
    POP,             // discard top
    DUP,             // duplicate top

    // ── Variables ────────────────────────────────────────────────────
    GET_LOCAL,       // [index]   push locals[index]
    SET_LOCAL,       // [index]   locals[index] = top (no pop)
    GET_GLOBAL,      // [hi, lo]  push globals[name_constant]
    DEFINE_GLOBAL,   // [hi, lo]  globals[name_constant] = pop
    SET_GLOBAL,      // [hi, lo]  globals[name_constant] = top (no pop)

    // ── Arithmetic ───────────────────────────────────────────────────
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    MODULO,
    NEGATE,

    // ── Logic ────────────────────────────────────────────────────────
    NOT,             // boolean NOT

    // ── Comparison ───────────────────────────────────────────────────
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL,

    // ── Control flow ─────────────────────────────────────────────────
    JUMP,            // [hi, lo]  ip += offset unconditionally
    JUMP_IF_FALSE,   // [hi, lo]  ip += offset if top is falsy (no pop)
    JUMP_IF_TRUE,    // [hi, lo]  ip += offset if top is truthy (short-circuit)
    LOOP,            // [hi, lo]  ip -= offset (loop back)

    // ── Functions ────────────────────────────────────────────────────
    CALL,            // [argc]    call top-(argc) with argc args
    RETURN,          // return top value from current frame

    // ── I/O ──────────────────────────────────────────────────────────
    PRINT,           // pop & print with newline
    INPUT,           // [hi, lo]  print prompt constant, read line, push string

    // ── Misc ─────────────────────────────────────────────────────────
    HALT,

    _COUNT           // sentinel — keep last
};

const char* opcode_name(OpCode op);

// ── Chunk — one compiled unit (script or function body) ───────────────────────
struct Chunk {
    std::string              name;
    std::vector<uint8_t>     code;
    std::vector<Value>       constants;
    std::vector<int>         lines;      // parallel to code

    explicit Chunk(std::string n = "<script>") : name(std::move(n)) {}

    // Write one byte, recording the source line.
    void write(uint8_t byte, int line);

    // Write an opcode.
    void write_op(OpCode op, int line) { write(static_cast<uint8_t>(op), line); }

    // Add a constant; return its index (16-bit).
    int  add_constant(Value v);

    // Write CONSTANT + two-byte index in one call.
    void write_constant(Value v, int line);

    // Patch a previously emitted 16-bit offset (for backpatching jumps).
    void patch_jump(int offset);

    int current_offset() const { return static_cast<int>(code.size()); }
};
