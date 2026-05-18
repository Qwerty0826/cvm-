#pragma once
#include "common.h"

// ── Forward declaration ───────────────────────────────────────────────────────
struct Chunk;

// ── Runtime value types ───────────────────────────────────────────────────────
enum class ValueType : uint8_t {
    NIL,
    BOOL,
    NUMBER,
    STRING,
    FUNCTION,
};

// A compiled function object (lives on the heap, ref-counted via shared_ptr).
struct Function {
    std::string   name;
    int           arity   = 0;
    std::shared_ptr<Chunk> chunk;

    explicit Function(std::string n = "<script>");
};

// ── Value ─────────────────────────────────────────────────────────────────────
class Value {
public:
    ValueType type = ValueType::NIL;

    // Payload ─────────────────────────────────────────
    union {
        bool   boolean;
        double number;
    };
    std::string                  string;   // only for STRING
    std::shared_ptr<Function>    fn;       // only for FUNCTION

    // Constructors ────────────────────────────────────
    Value() : type(ValueType::NIL), number(0) {}

    static Value nil()                              { Value v; v.type = ValueType::NIL;      v.number  = 0;   return v; }
    static Value from_bool(bool b)                  { Value v; v.type = ValueType::BOOL;     v.boolean = b;   return v; }
    static Value from_number(double n)              { Value v; v.type = ValueType::NUMBER;   v.number  = n;   return v; }
    static Value from_string(std::string s)         { Value v; v.type = ValueType::STRING;   v.string  = std::move(s); v.number = 0; return v; }
    static Value from_function(std::shared_ptr<Function> f) {
        Value v; v.type = ValueType::FUNCTION; v.fn = std::move(f); v.number = 0; return v;
    }

    // Type predicates ─────────────────────────────────
    bool is_nil()      const { return type == ValueType::NIL;      }
    bool is_bool()     const { return type == ValueType::BOOL;     }
    bool is_number()   const { return type == ValueType::NUMBER;   }
    bool is_string()   const { return type == ValueType::STRING;   }
    bool is_function() const { return type == ValueType::FUNCTION; }

    // Truthiness: nil and false are falsy, everything else truthy.
    bool is_truthy() const {
        if (is_nil())  return false;
        if (is_bool()) return boolean;
        return true;
    }

    // Human-readable representation ───────────────────
    std::string to_string() const;

    // Equality ────────────────────────────────────────
    bool operator==(const Value& o) const;
    bool operator!=(const Value& o) const { return !(*this == o); }
};

// Pretty-print a value to an ostream
std::ostream& operator<<(std::ostream& os, const Value& v);
