#include "common.h"
#include <unistd.h>
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "disassembler.h"
#include "ast_printer.h"
#include "optimizer.h"
#include "vm.h"
#include "repl.h"
#include "serializer.h"
#include <filesystem>

// ── CLI Usage ─────────────────────────────────────────────────────────────────
static void usage(const char* prog) {
    std::cout << Color::BOLD() << "CVM++ v" << CVM_VERSION_MAJOR
              << "." << CVM_VERSION_MINOR << "." << CVM_VERSION_PATCH
              << Color::RESET() << "  —  A Stack-Based VM & Custom Compiler\n\n"
              << Color::BOLD() << "USAGE\n" << Color::RESET()
              << "  " << prog << " [options] [file.cvm | file.cvmb]\n\n"
              << Color::BOLD() << "OPTIONS\n" << Color::RESET()
              << "  --repl           Start interactive REPL (default when no file given)\n"
              << "  --dump-ast       Print the AST before compiling\n"
              << "  --dump-bytecode  Print disassembled bytecode\n"
              << "  --trace          Trace VM execution (step-by-step)\n"
              << "  --no-optimize    Disable constant-folding optimizer\n"
              << "  --profile        Show instruction execution profile after run\n"
              << "  --compile-only   Compile to .cvmb and exit (no execution)\n"
              << "  --output <file>  Output file for --compile-only (default: <input>.cvmb)\n"
              << "  --run <file>     Run a pre-compiled .cvmb file\n"
              << "  --no-color       Disable ANSI color output\n"
              << "  --version        Print version and exit\n"
              << "  --help           Show this help\n\n"
              << Color::BOLD() << "EXAMPLES\n" << Color::RESET()
              << "  " << prog << " script.cvm                  # compile & run\n"
              << "  " << prog << " --dump-bytecode script.cvm  # show bytecode then run\n"
              << "  " << prog << " --compile-only script.cvm   # produce script.cvmb\n"
              << "  " << prog << " --run script.cvmb           # run pre-compiled bytecode\n"
              << "  " << prog << "                             # start REPL\n";
}

// ── Error pretty-printing with source context ─────────────────────────────────
static void print_error(const ParseError& e) {
    std::cerr << Color::RED() << Color::BOLD()
              << "error" << Color::RESET()
              << "[line " << e.line << "]: " << e.message << "\n";
    if (!e.context.empty()) {
        std::cerr << "  " << e.context << "\n";
        std::cerr << "  " << Color::RED() << "^~~~" << Color::RESET() << "\n";
    }
}

// ── Source file runner ────────────────────────────────────────────────────────
static int run_source_file(const std::string& path,
                           bool dump_ast, bool dump_bytecode,
                           bool trace, bool optimize, bool profile_mode,
                           bool compile_only, const std::string& out_path) {
    // Read source
    std::ifstream f(path);
    if (!f) {
        std::cerr << Color::RED() << "Cannot open file: " << path << Color::RESET() << "\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // Lex
    Lexer lexer(source);
    auto tokens = lexer.tokenise();
    bool lex_err = false;
    for (auto& t : tokens) {
        if (t.type == TokenType::ERROR) {
            std::cerr << Color::RED() << "Lex error [line " << t.line << "]: "
                      << t.lexeme << Color::RESET() << "\n";
            lex_err = true;
        }
    }
    if (lex_err) return 1;

    // Parse
    Parser parser(std::move(tokens), source);
    Program prog = parser.parse();
    if (parser.had_error()) {
        for (auto& e : parser.errors()) print_error(e);
        return 1;
    }

    // Optimize
    if (optimize) {
        Optimizer opt;
        int folds = opt.fold(prog);
        if (folds > 0 && (dump_ast || dump_bytecode))
            std::cout << Color::YELLOW() << "[optimizer: " << folds
                      << " constant fold(s) applied]\n" << Color::RESET();
    }

    // AST dump
    if (dump_ast) {
        AstPrinter printer(std::cout);
        printer.print(prog);
    }

    // Compile
    Compiler compiler;
    auto fn = compiler.compile(prog);
    if (compiler.had_error()) return 1;

    // Bytecode dump
    if (dump_bytecode) {
        Disassembler::disassemble(*fn->chunk, std::cout);
    }

    // Serialize
    if (compile_only) {
        std::string dest = out_path.empty()
            ? std::filesystem::path(path).replace_extension(".cvmb").string()
            : out_path;
        try {
            Serializer::save(*fn, dest);
            std::cout << Color::GREEN() << "Compiled to: " << dest << Color::RESET() << "\n";
        } catch (const std::exception& ex) {
            std::cerr << Color::RED() << "Serialization error: " << ex.what() << Color::RESET() << "\n";
            return 1;
        }
        return 0;
    }

    // Execute
    VM vm(trace);
    if (profile_mode) vm.profile().enabled = true;

    auto result = vm.run(fn);

    if (profile_mode) vm.profile().print(std::cout);

    return (result == InterpretResult::OK) ? 0 : 1;
}

// ── Pre-compiled .cvmb runner ─────────────────────────────────────────────────
static int run_bytecode_file(const std::string& path,
                             bool dump_bytecode, bool trace,
                             bool profile_mode) {
    std::shared_ptr<Function> fn;
    try {
        fn = Serializer::load(path);
    } catch (const std::exception& ex) {
        std::cerr << Color::RED() << "Load error: " << ex.what() << Color::RESET() << "\n";
        return 1;
    }

    if (dump_bytecode) Disassembler::disassemble(*fn->chunk, std::cout);

    VM vm(trace);
    if (profile_mode) vm.profile().enabled = true;
    auto result = vm.run(fn);
    if (profile_mode) vm.profile().print(std::cout);
    return (result == InterpretResult::OK) ? 0 : 1;
}

// ── main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    bool        dump_ast      = false;
    bool        dump_bytecode = false;
    bool        trace         = false;
    bool        optimize      = true;
    bool        profile_mode  = false;
    bool        compile_only  = false;
    bool        start_repl    = false;
    std::string input_file;
    std::string output_file;
    std::string run_file;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if      (arg == "--help"         || arg == "-h") { usage(argv[0]); return 0; }
        else if (arg == "--version"      || arg == "-v") {
            std::cout << "CVM++ " << CVM_VERSION_MAJOR << "."
                      << CVM_VERSION_MINOR << "." << CVM_VERSION_PATCH << "\n";
            return 0;
        }
        else if (arg == "--dump-ast")      dump_ast      = true;
        else if (arg == "--dump-bytecode") dump_bytecode = true;
        else if (arg == "--trace")         trace         = true;
        else if (arg == "--no-optimize")   optimize      = false;
        else if (arg == "--profile")       profile_mode  = true;
        else if (arg == "--compile-only")  compile_only  = true;
        else if (arg == "--repl")          start_repl    = true;
        else if (arg == "--no-color")      Color::enabled = false;
        else if (arg == "--output") {
            if (++i >= argc) { std::cerr << "--output requires a filename\n"; return 1; }
            output_file = argv[i];
        }
        else if (arg == "--run") {
            if (++i >= argc) { std::cerr << "--run requires a filename\n"; return 1; }
            run_file = argv[i];
        }
        else if (arg[0] == '-') {
            std::cerr << Color::RED() << "Unknown option: " << arg << Color::RESET() << "\n";
            return 1;
        }
        else {
            input_file = arg;
        }
    }

    // Check if terminal supports color
    if (!isatty(fileno(stdout))) Color::enabled = false;

    // --run: execute a pre-compiled .cvmb
    if (!run_file.empty()) {
        return run_bytecode_file(run_file, dump_bytecode, trace, profile_mode);
    }

    // Source file given
    if (!input_file.empty()) {
        // Auto-detect .cvmb
        if (input_file.size() >= 5 &&
            input_file.substr(input_file.size() - 5) == ".cvmb") {
            return run_bytecode_file(input_file, dump_bytecode, trace, profile_mode);
        }
        return run_source_file(input_file, dump_ast, dump_bytecode,
                               trace, optimize, profile_mode,
                               compile_only, output_file);
    }

    // Default: REPL
    ReplOptions ropts;
    ropts.dump_ast      = dump_ast;
    ropts.dump_bytecode = dump_bytecode;
    ropts.trace         = trace;
    ropts.optimize      = optimize;
    Repl repl(ropts);
    repl.run();
    return 0;
}
