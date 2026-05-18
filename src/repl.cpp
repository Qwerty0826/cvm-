#include "repl.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "disassembler.h"
#include "ast_printer.h"
#include "optimizer.h"

Repl::Repl(ReplOptions opts)
    : opts_(opts), vm_(opts.trace) {}

void Repl::print_banner() {
    std::cout << Color::BOLD() << Color::CYAN()
              << "CVM++ v" << CVM_VERSION_MAJOR << "." << CVM_VERSION_MINOR << "." << CVM_VERSION_PATCH
              << Color::RESET() << "  —  Stack-Based VM & Compiler\n"
              << "Type " << Color::YELLOW() << ":help" << Color::RESET() << " for commands, "
              << Color::YELLOW() << ":quit" << Color::RESET() << " to exit.\n\n";
}

void Repl::run() {
    print_banner();

    std::string input;
    while (true) {
        std::cout << Color::GREEN() << ">>> " << Color::RESET() << std::flush;
        if (!std::getline(std::cin, input)) break;

        // Trim
        auto it = input.begin();
        while (it != input.end() && std::isspace((unsigned char)*it)) ++it;
        std::string trimmed(it, input.end());

        if (trimmed.empty()) continue;

        // REPL commands
        if (trimmed == ":quit" || trimmed == ":q") break;
        if (trimmed == ":help") {
            std::cout << Color::YELLOW()
                      << "  :quit         exit the REPL\n"
                      << "  :ast          toggle AST dump\n"
                      << "  :bytecode     toggle bytecode dump\n"
                      << "  :trace        toggle execution trace\n"
                      << "  :optimize     toggle constant folding\n"
                      << "  :clear        reset VM state\n"
                      << Color::RESET();
            continue;
        }
        if (trimmed == ":ast")      { opts_.dump_ast      = !opts_.dump_ast;      std::cout << "AST dump "      << (opts_.dump_ast?"on":"off")      << "\n"; continue; }
        if (trimmed == ":bytecode") { opts_.dump_bytecode = !opts_.dump_bytecode; std::cout << "Bytecode dump " << (opts_.dump_bytecode?"on":"off") << "\n"; continue; }
        if (trimmed == ":trace")    { opts_.trace         = !opts_.trace;         vm_ = VM(opts_.trace);         std::cout << "Trace "         << (opts_.trace?"on":"off")         << "\n"; continue; }
        if (trimmed == ":optimize") { opts_.optimize      = !opts_.optimize;      std::cout << "Optimizer "     << (opts_.optimize?"on":"off")     << "\n"; continue; }
        if (trimmed == ":clear")    { vm_.reset(); std::cout << "VM state cleared.\n"; continue; }

        eval_line(trimmed);
    }

    std::cout << "\nBye!\n";
}

void Repl::eval_line(const std::string& source) {
    // Lex
    Lexer lexer(source);
    auto tokens = lexer.tokenise();
    for (auto& t : tokens) {
        if (t.type == TokenType::ERROR) {
            std::cerr << Color::RED() << "Lex error: " << t.lexeme << Color::RESET() << "\n";
            return;
        }
    }

    // Parse
    Parser parser(std::move(tokens), source);
    Program prog = parser.parse();
    if (parser.had_error()) {
        for (auto& e : parser.errors()) {
            std::cerr << Color::RED() << "Parse error [line " << e.line << "]: "
                      << e.message << Color::RESET() << "\n";
            if (!e.context.empty()) {
                std::cerr << "  " << e.context << "\n";
                std::cerr << "  " << Color::RED() << "^" << Color::RESET() << "\n";
            }
        }
        return;
    }

    // Optimize
    if (opts_.optimize) {
        Optimizer opt;
        int folds = opt.fold(prog);
        if (folds > 0)
            std::cout << Color::YELLOW() << "[optimizer: " << folds << " fold(s)]" << Color::RESET() << "\n";
    }

    // AST dump
    if (opts_.dump_ast) {
        AstPrinter printer(std::cout);
        printer.print(prog);
    }

    // Compile
    Compiler compiler;
    auto fn = compiler.compile(prog);
    if (compiler.had_error()) return;

    // Bytecode dump
    if (opts_.dump_bytecode) {
        Disassembler::disassemble(*fn->chunk, std::cout);
    }

    // Execute
    vm_.run(fn);
}
