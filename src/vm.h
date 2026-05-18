#pragma once
#include "common.h"
#include "chunk.h"
#include "value.h"

enum class InterpretResult { OK, RUNTIME_ERROR };

// Execution profile: counts how many times each opcode fires.
struct Profile {
    uint64_t counts[static_cast<int>(OpCode::_COUNT)] = {};
    bool     enabled = false;

    void record(OpCode op) {
        if (enabled) counts[static_cast<int>(op)]++;
    }
    void print(std::ostream& out) const;
};

// ── Call frame ────────────────────────────────────────────────────────────────
struct CallFrame {
    std::shared_ptr<Function> function;
    const uint8_t*            ip;      // instruction pointer into chunk's code
    Value*                    slots;   // base of this frame's stack window
};

// Native function type: receives argc values starting at args[0].
using NativeFn = std::function<Value(int argc, Value* args)>;

struct NativeFunction {
    std::string name;
    int         arity;  // -1 = variadic
    NativeFn    fn;
};

// ── Virtual Machine ───────────────────────────────────────────────────────────
class VM {
public:
    explicit VM(bool trace = false);

    // Execute a compiled top-level function.
    InterpretResult run(std::shared_ptr<Function> fn);

    // Expose profile data.
    Profile& profile() { return profile_; }

    // Reset state (for REPL sessions).
    void reset();

private:
    Value       stack_[STACK_MAX];
    Value*      stack_top_ = stack_;
    CallFrame   frames_[FRAMES_MAX];
    int         frame_count_ = 0;

    std::unordered_map<std::string, Value>          globals_;
    std::unordered_map<std::string, NativeFunction> natives_;

    Profile     profile_;
    bool        trace_;    // print each instruction as executed

    CallFrame& current_frame() { return frames_[frame_count_ - 1]; }

    // Stack operations
    void   push(Value v);
    Value  pop();
    Value& peek(int distance = 0);

    bool   call(Value callee, int argc);
    bool   call_native(const NativeFunction& native, int argc);
    void   runtime_error(const std::string& msg);
    void   register_natives();

    InterpretResult execute();
};
