#pragma once
#include "common.h"
#include "chunk.h"
#include "value.h"

// ── .cvmb Binary Bytecode Format ─────────────────────────────────────────────
//
// Magic:      4 bytes  "CVM\x00"
// Version:    2 bytes  (major << 8 | minor)
// Chunks:     serialized recursively
//
// Chunk layout:
//   name_len  u16
//   name      name_len bytes
//   const_cnt u16
//   constants: value* const_cnt
//     value:
//       type  u8  (0=nil, 1=bool, 2=number, 3=string, 4=function/chunk)
//       payload…
//   code_cnt  u32
//   code      code_cnt bytes
//   line_cnt  u32  (should equal code_cnt)
//   lines     line_cnt × i32
class Serializer {
public:
    // Write a compiled function to a .cvmb binary file.
    static void save(const Function& fn, const std::string& path);

    // Load a .cvmb file back into a Function.
    static std::shared_ptr<Function> load(const std::string& path);

private:
    // ── Write helpers ─────────────────────────────────────────────────
    static void write_u8 (std::ostream& out, uint8_t  v);
    static void write_u16(std::ostream& out, uint16_t v);
    static void write_u32(std::ostream& out, uint32_t v);
    static void write_f64(std::ostream& out, double   v);
    static void write_str(std::ostream& out, const std::string& s);
    static void write_value(std::ostream& out, const Value& v);
    static void write_chunk(std::ostream& out, const Chunk& c);

    // ── Read helpers ──────────────────────────────────────────────────
    static uint8_t     read_u8 (std::istream& in);
    static uint16_t    read_u16(std::istream& in);
    static uint32_t    read_u32(std::istream& in);
    static double      read_f64(std::istream& in);
    static std::string read_str(std::istream& in);
    static Value       read_value(std::istream& in);
    static std::shared_ptr<Chunk> read_chunk(std::istream& in);
};
