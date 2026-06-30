# CVM++

**CVM++ is a fully self-contained scripting language toolchain built from scratch in C++17 — no LLVM, no Flex, no Bison, no external libraries of any kind.**

It includes a hand-written lexer, a recursive-descent parser, a typed Abstract Syntax Tree, a constant-folding optimizer, a single-pass bytecode compiler, a stack-based virtual machine with a 33-opcode ISA, a binary bytecode serializer, a disassembler, an execution profiler, and an interactive REPL — all implemented across ~3,300 lines of C++.

The language, called CVM, supports integers, booleans, strings, user-defined recursive functions, lexically-scoped variables, `if/else`, `while`, `for`, and a built-in standard library of 11 native functions. Source files (`.cvm`) are compiled into a proprietary binary bytecode format (`.cvmb`) that can be distributed and executed independently of the source, in the same way Java `.class` files or Python `.pyc` files work.

The goal of the project is to demystify every layer of a language runtime — from the moment raw text is read off disk to the moment a result is printed — by building each layer by hand and making every intermediate representation inspectable at the command line.

```
Source (.cvm)  →  Lexer  →  Parser  →  Optimizer  →  Compiler  →  Bytecode  →  VM
                                                          ↓
                                               .cvmb binary file
```

---

## Features

| Layer | What's built |
|---|---|
| **Lexer** | Full tokeniser — all operators, escape sequences, nested block comments |
| **Parser** | Recursive-descent, typed AST, Rust-style errors with source context |
| **Optimizer** | Constant-folding pass (numbers, booleans, strings) before compilation |
| **Compiler** | Single-pass AST → bytecode with scope-aware local/global variable tracking |
| **VM** | Stack-based execution engine — 512-slot value stack, 64 call frames |
| **Disassembler** | Human-readable bytecode dump with offsets, lines, constants |
| **AST Printer** | Indented tree view for `--dump-ast` mode |
| **Serializer** | Binary `.cvmb` bytecode format — compile once, run anywhere |
| **REPL** | Interactive session with persistent state and toggle commands |
| **Native Stdlib** | `to_num`, `to_str`, `type_of`, `sqrt`, `abs`, `floor`, `ceil`, `len`, `max`, `min`, `pow` |

---

## Language Quick-Start

```cvm
// Variables
let x = 42;
let name = "world";
let flag = true;

// Arithmetic and string concatenation
print x * 2 + 1;          // 85
print "Hello, " + name;   // Hello, world

// Control flow
if (x > 40) {
    print "big";
} else {
    print "small";
}

// While loop
let i = 0;
while (i < 5) {
    print i;
    i = i + 1;
}

// For loop
for (let j = 0; j < 5; j = j + 1) {
    print j * j;
}

// Functions (with recursion)
fn factorial(n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}
print factorial(10);   // 3628800

// User input
let raw = input("Enter a number: ");
let n   = to_num(raw);
print n * n;
```

### Supported types
- **nil** — absence of value
- **bool** — `true` / `false`
- **number** — IEEE 754 double
- **string** — UTF-8, escape sequences (`\n`, `\t`, `\"`, `\\`)
- **function** — first-class, user-defined

### Operators
`+` `-` `*` `/` `%` `==` `!=` `<` `<=` `>` `>=` `and` `or` `not` `!` `-` (unary)

---

## Build

```bash
cmake -B build
cmake --build build
```

The binary is `build/cvm`.

**Requirements:** C++17 compiler, CMake ≥ 3.16.

---

## Usage

```
cvm [options] [file.cvm | file.cvmb]
```

| Flag | Effect |
|---|---|
| *(no args)* | Start interactive REPL |
| `file.cvm` | Compile and run a source file |
| `file.cvmb` | Run a pre-compiled bytecode file |
| `--dump-ast` | Print the AST before compiling |
| `--dump-bytecode` | Print disassembled bytecode |
| `--trace` | Step-by-step VM execution trace |
| `--profile` | Show per-opcode execution counts after run |
| `--no-optimize` | Disable constant-folding optimizer |
| `--compile-only` | Produce `.cvmb` file only (no execution) |
| `--output file` | Set output path for `--compile-only` |
| `--run file.cvmb` | Execute a pre-compiled bytecode file |
| `--no-color` | Disable ANSI colour output |

### Examples

```bash
# Run a script
./build/cvm samples/fizzbuzz.cvm

# Show AST + bytecode then run
./build/cvm --dump-ast --dump-bytecode samples/fibonacci.cvm

# Compile to bytecode file
./build/cvm --compile-only samples/calculator.cvm
# → samples/calculator.cvmb

# Run the compiled bytecode
./build/cvm --run samples/calculator.cvmb

# Profile instruction execution
./build/cvm --profile samples/functions_demo.cvm

# Interactive REPL
./build/cvm
```

### REPL commands

| Command | Effect |
|---|---|
| `:ast` | Toggle AST dump on/off |
| `:bytecode` | Toggle bytecode dump on/off |
| `:trace` | Toggle VM trace on/off |
| `:optimize` | Toggle constant-folding on/off |
| `:clear` | Reset VM state (clear globals) |
| `:quit` / `:q` | Exit |

---

## Architecture

```
src/
├── common.h          — shared includes, VM limits, ANSI colour helpers
├── value.{h,cpp}     — Value tagged-union + Function object
├── chunk.{h,cpp}     — Chunk (bytecode container) + OpCode ISA
├── lexer.{h,cpp}     — tokeniser
├── ast.h             — full AST node hierarchy (visitor pattern)
├── parser.{h,cpp}    — recursive-descent parser
├── compiler.{h,cpp}  — AST → bytecode compiler
├── disassembler.{h,cpp} — bytecode → human-readable dump
├── ast_printer.{h,cpp}  — AST → indented tree dump
├── optimizer.{h,cpp} — constant-folding AST pass
├── serializer.{h,cpp}— .cvmb binary read/write
├── vm.{h,cpp}        — stack-based execution engine
├── repl.{h,cpp}      — interactive REPL
└── main.cpp          — CLI driver
```

### Instruction Set (ISA)

| Opcode | Operands | Description |
|---|---|---|
| `CONSTANT` | hi, lo | push constants[hi<<8\|lo] |
| `NIL/TRUE/FALSE` | — | push literal |
| `POP` / `DUP` | — | stack manipulation |
| `GET/SET_LOCAL` | slot | local variable access |
| `GET/SET/DEFINE_GLOBAL` | hi, lo | global variable access |
| `ADD/SUBTRACT/MULTIPLY/DIVIDE/MODULO` | — | arithmetic |
| `NEGATE/NOT` | — | unary operators |
| `EQUAL/NOT_EQUAL/LESS/LESS_EQUAL/GREATER/GREATER_EQUAL` | — | comparison |
| `JUMP/JUMP_IF_FALSE/JUMP_IF_TRUE` | hi, lo | control flow |
| `LOOP` | hi, lo | loop back |
| `CALL` | argc | call function |
| `RETURN` | — | return from function |
| `PRINT` | — | print top of stack |
| `INPUT` | hi, lo | print prompt, read line |
| `HALT` | — | stop execution |

---

## Sample Programs

```bash
echo "15"    | ./build/cvm samples/fizzbuzz.cvm
echo "0"     | ./build/cvm samples/truth_machine.cvm
echo "8"     | ./build/cvm samples/fibonacci.cvm
./build/cvm samples/functions_demo.cvm
```

---

## .cvmb Binary Format

```
Magic:    4 bytes  "CVM\0"
Version:  2 bytes  major<<8 | minor
Arity:    1 byte
Chunk:
  name_len  u16
  name      name_len bytes
  const_cnt u16
  constants value*
  code_cnt  u32
  code      bytes
  line_cnt  u32
  lines     i32*
```

Values are type-tagged; function values embed their chunk recursively.

---

## Demo Video

[Watch the demo on Google Drive](https://drive.google.com/file/d/1p9vXArr47c96DFmpr_bMe820_DMWM7c2/view?usp=sharing)

---

**Pragadeesh S K**
