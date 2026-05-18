#include "vm.h"
#include "disassembler.h"
#include <cmath>
#include <sstream>
#include <iomanip>
#include <stdexcept>

// ── Profile ───────────────────────────────────────────────────────────────────
void Profile::print(std::ostream& out) const {
    out << Color::CYAN() << "══ Execution Profile ══" << Color::RESET() << "\n";
    out << Color::BOLD() << std::left << std::setw(22) << "OPCODE"
        << " EXECUTIONS\n" << Color::RESET();
    out << std::string(36, '-') << "\n";

    // Sort by execution count descending
    std::vector<std::pair<uint64_t, const char*>> rows;
    for (int i = 0; i < static_cast<int>(OpCode::_COUNT); ++i) {
        if (counts[i] > 0)
            rows.push_back({counts[i], opcode_name(static_cast<OpCode>(i))});
    }
    std::sort(rows.begin(), rows.end(), [](auto& a, auto& b){ return a.first > b.first; });
    for (auto& [cnt, name] : rows)
        out << std::left << std::setw(22) << name << " " << cnt << "\n";
    out << "\n";
}

// ── Native standard library ───────────────────────────────────────────────────
void VM::register_natives() {
    auto reg = [&](const char* name, int arity, NativeFn fn) {
        natives_[name] = NativeFunction{name, arity, std::move(fn)};
    };

    reg("to_num", 1, [](int, Value* args) -> Value {
        if (args[0].is_number()) return args[0];
        if (args[0].is_bool())   return Value::from_number(args[0].boolean ? 1.0 : 0.0);
        if (args[0].is_string()) {
            try { return Value::from_number(std::stod(args[0].string)); }
            catch (...) { return Value::nil(); }
        }
        return Value::nil();
    });

    reg("to_str", 1, [](int, Value* args) -> Value {
        return Value::from_string(args[0].to_string());
    });

    reg("type_of", 1, [](int, Value* args) -> Value {
        switch (args[0].type) {
            case ValueType::NIL:      return Value::from_string("nil");
            case ValueType::BOOL:     return Value::from_string("bool");
            case ValueType::NUMBER:   return Value::from_string("number");
            case ValueType::STRING:   return Value::from_string("string");
            case ValueType::FUNCTION: return Value::from_string("function");
        }
        return Value::from_string("unknown");
    });

    reg("sqrt", 1, [](int, Value* args) -> Value {
        if (!args[0].is_number()) return Value::nil();
        return Value::from_number(std::sqrt(args[0].number));
    });

    reg("abs", 1, [](int, Value* args) -> Value {
        if (!args[0].is_number()) return Value::nil();
        return Value::from_number(std::abs(args[0].number));
    });

    reg("floor", 1, [](int, Value* args) -> Value {
        if (!args[0].is_number()) return Value::nil();
        return Value::from_number(std::floor(args[0].number));
    });

    reg("ceil", 1, [](int, Value* args) -> Value {
        if (!args[0].is_number()) return Value::nil();
        return Value::from_number(std::ceil(args[0].number));
    });

    reg("len", 1, [](int, Value* args) -> Value {
        if (args[0].is_string()) return Value::from_number((double)args[0].string.size());
        return Value::nil();
    });

    reg("max", 2, [](int, Value* args) -> Value {
        if (!args[0].is_number() || !args[1].is_number()) return Value::nil();
        return Value::from_number(std::max(args[0].number, args[1].number));
    });

    reg("min", 2, [](int, Value* args) -> Value {
        if (!args[0].is_number() || !args[1].is_number()) return Value::nil();
        return Value::from_number(std::min(args[0].number, args[1].number));
    });

    reg("pow", 2, [](int, Value* args) -> Value {
        if (!args[0].is_number() || !args[1].is_number()) return Value::nil();
        return Value::from_number(std::pow(args[0].number, args[1].number));
    });

    // Expose all natives as global values (dummy function values for call dispatch)
    for (auto& [name, native] : natives_) {
        (void)name; // accessed via name key
    }
}

// ── VM construction ───────────────────────────────────────────────────────────
VM::VM(bool trace) : trace_(trace) {
    stack_top_ = stack_;
    frame_count_ = 0;
    register_natives();
}

void VM::reset() {
    stack_top_  = stack_;
    frame_count_ = 0;
    // keep globals across REPL lines
}

// ── Stack ─────────────────────────────────────────────────────────────────────
void VM::push(Value v) {
    if (stack_top_ - stack_ >= STACK_MAX)
        runtime_error("Stack overflow.");
    *stack_top_++ = std::move(v);
}

Value VM::pop() {
    if (stack_top_ == stack_)
        runtime_error("Stack underflow.");
    return std::move(*--stack_top_);
}

Value& VM::peek(int distance) {
    return *(stack_top_ - 1 - distance);
}

// ── Runtime error ─────────────────────────────────────────────────────────────
void VM::runtime_error(const std::string& msg) {
    std::ostringstream oss;
    oss << Color::RED() << "Runtime error: " << Color::RESET() << msg << "\n";

    // Print a stack trace
    for (int i = frame_count_ - 1; i >= 0; --i) {
        auto& frame = frames_[i];
        const Chunk& chunk = *frame.function->chunk;
        int offset = static_cast<int>(frame.ip - chunk.code.data()) - 1;
        int line   = (offset >= 0 && offset < (int)chunk.lines.size())
                     ? chunk.lines[offset] : 0;
        oss << "  [line " << line << "] in "
            << (frame.function->name.empty() ? "<script>" : frame.function->name)
            << "\n";
    }
    std::cerr << oss.str();
    throw std::runtime_error(msg);
}

// ── Native call ───────────────────────────────────────────────────────────────
bool VM::call_native(const NativeFunction& native, int argc) {
    if (native.arity >= 0 && native.arity != argc) {
        runtime_error("Native '" + native.name + "' expects " +
                      std::to_string(native.arity) + " args, got " +
                      std::to_string(argc) + ".");
        return false;
    }
    Value* args = stack_top_ - argc;
    Value result = native.fn(argc, args);
    stack_top_ -= argc + 1; // pop args + callee
    push(result);
    return true;
}

// ── Function call ─────────────────────────────────────────────────────────────
bool VM::call(Value callee, int argc) {
    if (!callee.is_function()) {
        runtime_error("Can only call functions, got '" + callee.to_string() + "'.");
        return false;
    }
    auto fn = callee.fn;
    if (fn->arity != argc) {
        runtime_error("Expected " + std::to_string(fn->arity) +
                      " arguments but got " + std::to_string(argc) + ".");
        return false;
    }
    if (frame_count_ >= FRAMES_MAX) {
        runtime_error("Call stack overflow (max depth " + std::to_string(FRAMES_MAX) + ").");
        return false;
    }
    CallFrame& frame = frames_[frame_count_++];
    frame.function = fn;
    frame.ip       = fn->chunk->code.data();
    // Slot 0 is the function value itself (already on stack before args).
    frame.slots    = stack_top_ - argc - 1;
    return true;
}

// ── Main execution loop ───────────────────────────────────────────────────────
InterpretResult VM::run(std::shared_ptr<Function> fn) {
    push(Value::from_function(fn));
    call(peek(0), 0);

    try {
        return execute();
    } catch (const std::runtime_error&) {
        stack_top_   = stack_;
        frame_count_ = 0;
        return InterpretResult::RUNTIME_ERROR;
    }
}

// Macros for fast operand reading within the hot loop
#define READ_BYTE()   (*frame->ip++)
#define READ_SHORT()  (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONST()  (chunk->constants[READ_SHORT()])

InterpretResult VM::execute() {
    CallFrame* frame = &frames_[frame_count_ - 1];
    Chunk*     chunk = frame->function->chunk.get();

    while (true) {
        if (trace_) {
            // Print current stack
            std::cout << Color::YELLOW() << "          [ ";
            for (Value* s = stack_; s < stack_top_; s++)
                std::cout << *s << " ";
            std::cout << "]" << Color::RESET() << "\n";
            int offset = static_cast<int>(frame->ip - chunk->code.data());
            Disassembler::disassemble_instruction(*chunk, offset, std::cout);
        }

        uint8_t byte = READ_BYTE();
        OpCode  op   = static_cast<OpCode>(byte);
        profile_.record(op);

        switch (op) {

        // ── Constants ──────────────────────────────────────────────────────
        case OpCode::CONSTANT: {
            push(READ_CONST());
            break;
        }
        case OpCode::NIL:   push(Value::nil());          break;
        case OpCode::TRUE:  push(Value::from_bool(true)); break;
        case OpCode::FALSE: push(Value::from_bool(false));break;

        // ── Stack ──────────────────────────────────────────────────────────
        case OpCode::POP: pop(); break;
        case OpCode::DUP: push(peek(0)); break;

        // ── Local variables ────────────────────────────────────────────────
        case OpCode::GET_LOCAL: {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
            break;
        }
        case OpCode::SET_LOCAL: {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0); // leave value on stack
            break;
        }

        // ── Global variables ───────────────────────────────────────────────
        case OpCode::GET_GLOBAL: {
            Value name_val = READ_CONST();
            auto it = globals_.find(name_val.string);
            if (it == globals_.end()) {
                // Check native functions
                if (natives_.count(name_val.string)) {
                    push(name_val); // push the name as a sentinel for CALL
                    break;
                }
                runtime_error("Undefined variable '" + name_val.string + "'.");
            }
            push(it->second);
            break;
        }
        case OpCode::DEFINE_GLOBAL: {
            Value name_val = READ_CONST();
            globals_[name_val.string] = pop();
            break;
        }
        case OpCode::SET_GLOBAL: {
            Value name_val = READ_CONST();
            if (globals_.find(name_val.string) == globals_.end())
                runtime_error("Undefined variable '" + name_val.string + "'.");
            globals_[name_val.string] = peek(0);
            break;
        }

        // ── Arithmetic ─────────────────────────────────────────────────────
        case OpCode::ADD: {
            Value b = pop(), a = pop();
            if (a.is_number() && b.is_number()) {
                push(Value::from_number(a.number + b.number));
            } else if (a.is_string() || b.is_string()) {
                push(Value::from_string(a.to_string() + b.to_string()));
            } else {
                runtime_error("Operands to '+' must be numbers or strings.");
            }
            break;
        }

#define BINARY_OP(result_ctor, op)                                  \
    do {                                                             \
        Value b = pop(), a = pop();                                  \
        if (!a.is_number() || !b.is_number())                        \
            runtime_error("Operands must be numbers.");              \
        push(result_ctor(a.number op b.number));                    \
    } while (false)

        case OpCode::SUBTRACT: BINARY_OP(Value::from_number, -); break;
        case OpCode::MULTIPLY: BINARY_OP(Value::from_number, *); break;
        case OpCode::DIVIDE:
            if (peek(0).is_number() && peek(0).number == 0)
                runtime_error("Division by zero.");
            BINARY_OP(Value::from_number, /);
            break;
        case OpCode::MODULO: {
            Value b = pop(), a = pop();
            if (!a.is_number() || !b.is_number())
                runtime_error("Operands must be numbers.");
            if (b.number == 0) runtime_error("Modulo by zero.");
            push(Value::from_number(std::fmod(a.number, b.number)));
            break;
        }
        case OpCode::NEGATE: {
            Value v = pop();
            if (!v.is_number()) runtime_error("Operand must be a number.");
            push(Value::from_number(-v.number));
            break;
        }
        case OpCode::NOT: {
            Value v = pop();
            push(Value::from_bool(!v.is_truthy()));
            break;
        }

        // ── Comparisons ────────────────────────────────────────────────────
        case OpCode::EQUAL: {
            Value b = pop(), a = pop();
            push(Value::from_bool(a == b));
            break;
        }
        case OpCode::NOT_EQUAL: {
            Value b = pop(), a = pop();
            push(Value::from_bool(a != b));
            break;
        }

#define COMPARE_OP(op)                                               \
    do {                                                             \
        Value b = pop(), a = pop();                                  \
        if (!a.is_number() || !b.is_number())                        \
            runtime_error("Operands must be numbers for comparison.");\
        push(Value::from_bool(a.number op b.number));               \
    } while (false)

        case OpCode::LESS:          COMPARE_OP(<);  break;
        case OpCode::LESS_EQUAL:    COMPARE_OP(<=); break;
        case OpCode::GREATER:       COMPARE_OP(>);  break;
        case OpCode::GREATER_EQUAL: COMPARE_OP(>=); break;

#undef COMPARE_OP
#undef BINARY_OP

        // ── Control flow ───────────────────────────────────────────────────
        case OpCode::JUMP: {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OpCode::JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            if (!peek(0).is_truthy()) frame->ip += offset;
            break;
        }
        case OpCode::JUMP_IF_TRUE: {
            uint16_t offset = READ_SHORT();
            if (peek(0).is_truthy()) frame->ip += offset;
            break;
        }
        case OpCode::LOOP: {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }

        // ── Functions ──────────────────────────────────────────────────────
        case OpCode::CALL: {
            int argc = READ_BYTE();
            Value callee = peek(argc);

            // Check if it's a native function call (callee is a string name)
            if (callee.is_string()) {
                auto it = natives_.find(callee.string);
                if (it != natives_.end()) {
                    call_native(it->second, argc);
                    break;
                }
            }

            call(callee, argc);
            frame = &frames_[frame_count_ - 1];
            chunk = frame->function->chunk.get();
            break;
        }
        case OpCode::RETURN: {
            Value result = pop();
            frame_count_--;
            if (frame_count_ == 0) {
                pop(); // pop <script> function
                return InterpretResult::OK;
            }
            stack_top_ = frame->slots; // unwind stack to caller's base
            push(result);
            frame = &frames_[frame_count_ - 1];
            chunk = frame->function->chunk.get();
            break;
        }

        // ── I/O ────────────────────────────────────────────────────────────
        case OpCode::PRINT: {
            std::cout << pop() << "\n";
            break;
        }
        case OpCode::INPUT: {
            Value prompt_val = READ_CONST();
            if (!prompt_val.string.empty())
                std::cout << prompt_val.string << std::flush;
            std::string line;
            std::getline(std::cin, line);
            push(Value::from_string(line));
            break;
        }

        // ── Misc ───────────────────────────────────────────────────────────
        case OpCode::HALT:
            return InterpretResult::OK;

        default:
            runtime_error("Unknown opcode: " + std::to_string(byte));
        }
    }
}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONST
