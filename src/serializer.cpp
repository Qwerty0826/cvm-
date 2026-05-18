#include "serializer.h"
#include <cstring>
#include <stdexcept>

static constexpr char MAGIC[4] = {'C','V','M','\0'};

// ── Write helpers ─────────────────────────────────────────────────────────────
void Serializer::write_u8(std::ostream& out, uint8_t v) {
    out.write(reinterpret_cast<const char*>(&v), 1);
}
void Serializer::write_u16(std::ostream& out, uint16_t v) {
    uint8_t buf[2] = { uint8_t(v >> 8), uint8_t(v & 0xFF) };
    out.write(reinterpret_cast<const char*>(buf), 2);
}
void Serializer::write_u32(std::ostream& out, uint32_t v) {
    uint8_t buf[4] = { uint8_t(v>>24), uint8_t(v>>16), uint8_t(v>>8), uint8_t(v) };
    out.write(reinterpret_cast<const char*>(buf), 4);
}
void Serializer::write_f64(std::ostream& out, double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    for (int i = 7; i >= 0; --i)
        write_u8(out, (bits >> (i*8)) & 0xFF);
}
void Serializer::write_str(std::ostream& out, const std::string& s) {
    write_u16(out, static_cast<uint16_t>(s.size()));
    out.write(s.data(), s.size());
}

void Serializer::write_value(std::ostream& out, const Value& v) {
    write_u8(out, static_cast<uint8_t>(v.type));
    switch (v.type) {
        case ValueType::NIL:      break;
        case ValueType::BOOL:     write_u8(out, v.boolean ? 1 : 0); break;
        case ValueType::NUMBER:   write_f64(out, v.number); break;
        case ValueType::STRING:   write_str(out, v.string); break;
        case ValueType::FUNCTION:
            write_u8(out, static_cast<uint8_t>(v.fn->arity));
            write_chunk(out, *v.fn->chunk);
            break;
    }
}

void Serializer::write_chunk(std::ostream& out, const Chunk& c) {
    write_str(out, c.name);

    write_u16(out, static_cast<uint16_t>(c.constants.size()));
    for (auto& v : c.constants) write_value(out, v);

    write_u32(out, static_cast<uint32_t>(c.code.size()));
    out.write(reinterpret_cast<const char*>(c.code.data()), c.code.size());

    write_u32(out, static_cast<uint32_t>(c.lines.size()));
    for (int l : c.lines) {
        write_u32(out, static_cast<uint32_t>(l));
    }
}

void Serializer::save(const Function& fn, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open file for writing: " + path);
    out.write(MAGIC, 4);
    write_u16(out, (CVM_VERSION_MAJOR << 8) | CVM_VERSION_MINOR);
    write_u8(out, static_cast<uint8_t>(fn.arity));
    write_chunk(out, *fn.chunk);
}

// ── Read helpers ──────────────────────────────────────────────────────────────
uint8_t Serializer::read_u8(std::istream& in) {
    uint8_t v; in.read(reinterpret_cast<char*>(&v), 1); return v;
}
uint16_t Serializer::read_u16(std::istream& in) {
    uint8_t b[2]; in.read(reinterpret_cast<char*>(b), 2);
    return (uint16_t(b[0]) << 8) | b[1];
}
uint32_t Serializer::read_u32(std::istream& in) {
    uint8_t b[4]; in.read(reinterpret_cast<char*>(b), 4);
    return (uint32_t(b[0])<<24)|(uint32_t(b[1])<<16)|(uint32_t(b[2])<<8)|b[3];
}
double Serializer::read_f64(std::istream& in) {
    uint64_t bits = 0;
    for (int i = 7; i >= 0; --i) bits |= (uint64_t(read_u8(in)) << (i*8));
    double v; std::memcpy(&v, &bits, 8); return v;
}
std::string Serializer::read_str(std::istream& in) {
    uint16_t len = read_u16(in);
    std::string s(len, '\0');
    in.read(s.data(), len);
    return s;
}

Value Serializer::read_value(std::istream& in) {
    auto type = static_cast<ValueType>(read_u8(in));
    switch (type) {
        case ValueType::NIL:      return Value::nil();
        case ValueType::BOOL:     return Value::from_bool(read_u8(in) != 0);
        case ValueType::NUMBER:   return Value::from_number(read_f64(in));
        case ValueType::STRING:   return Value::from_string(read_str(in));
        case ValueType::FUNCTION: {
            int arity = read_u8(in);
            auto chunk = read_chunk(in);
            auto fn = std::make_shared<Function>(chunk->name);
            fn->arity = arity;
            fn->chunk = chunk;
            return Value::from_function(fn);
        }
    }
    return Value::nil();
}

std::shared_ptr<Chunk> Serializer::read_chunk(std::istream& in) {
    auto chunk = std::make_shared<Chunk>(read_str(in));
    uint16_t const_cnt = read_u16(in);
    chunk->constants.reserve(const_cnt);
    for (int i = 0; i < const_cnt; ++i)
        chunk->constants.push_back(read_value(in));

    uint32_t code_cnt = read_u32(in);
    chunk->code.resize(code_cnt);
    in.read(reinterpret_cast<char*>(chunk->code.data()), code_cnt);

    uint32_t line_cnt = read_u32(in);
    chunk->lines.resize(line_cnt);
    for (uint32_t i = 0; i < line_cnt; ++i)
        chunk->lines[i] = static_cast<int>(read_u32(in));

    return chunk;
}

std::shared_ptr<Function> Serializer::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path);

    char magic[4];
    in.read(magic, 4);
    if (std::memcmp(magic, MAGIC, 4) != 0)
        throw std::runtime_error("Not a valid .cvmb file: " + path);

    uint16_t version = read_u16(in);
    int major = version >> 8, minor = version & 0xFF;
    if (major != CVM_VERSION_MAJOR)
        throw std::runtime_error("Incompatible bytecode version " +
                                 std::to_string(major) + "." + std::to_string(minor));

    int arity = read_u8(in);
    auto chunk = read_chunk(in);
    auto fn = std::make_shared<Function>(chunk->name);
    fn->arity = arity;
    fn->chunk = chunk;
    return fn;
}
