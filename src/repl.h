#pragma once
#include "common.h"
#include "vm.h"

struct ReplOptions {
    bool dump_ast      = false;
    bool dump_bytecode = false;
    bool trace         = false;
    bool optimize      = true;
};

class Repl {
public:
    explicit Repl(ReplOptions opts = {});
    void run();

private:
    ReplOptions opts_;
    VM          vm_;

    void eval_line(const std::string& source);
    void print_banner();
};
