#include "value.h"
#include "chunk.h"
#include <cmath>
#include <sstream>
#include <iomanip>

Function::Function(std::string n)
    : name(std::move(n)), arity(0),
      chunk(std::make_shared<Chunk>(name)) {}

std::string Value::to_string() const {
    switch (type) {
        case ValueType::NIL:      return "nil";
        case ValueType::BOOL:     return boolean ? "true" : "false";
        case ValueType::NUMBER: {
            // Print integers without decimal point.
            if (number == std::floor(number) &&
                !std::isinf(number) && std::abs(number) < 1e15) {
                std::ostringstream oss;
                oss << static_cast<long long>(number);
                return oss.str();
            }
            std::ostringstream oss;
            oss << std::setprecision(10) << number;
            return oss.str();
        }
        case ValueType::STRING:   return string;
        case ValueType::FUNCTION:
            return fn ? "<fn " + fn->name + "/" + std::to_string(fn->arity) + ">"
                      : "<fn ?>";
    }
    return "?";
}

bool Value::operator==(const Value& o) const {
    if (type != o.type) return false;
    switch (type) {
        case ValueType::NIL:      return true;
        case ValueType::BOOL:     return boolean == o.boolean;
        case ValueType::NUMBER:   return number  == o.number;
        case ValueType::STRING:   return string  == o.string;
        case ValueType::FUNCTION: return fn.get() == o.fn.get();
    }
    return false;
}

std::ostream& operator<<(std::ostream& os, const Value& v) {
    return os << v.to_string();
}
