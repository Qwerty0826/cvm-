# CVM++ — Project Report

## Overview

CVM++ is a complete implementation of a custom scripting language, bytecode compiler, and stack-based virtual machine built from scratch in C++17. The project covers every stage of the compilation pipeline — from raw text to execution — with no third-party libraries beyond the C++ standard library.

---

## Architecture

The system is organised into seven independent but connected layers:

```
Source Text (.cvm)
      │
      ▼
   Lexer           Converts characters → Token stream
      │
      ▼
   Parser          Token stream → Abstract Syntax Tree (AST)
      │
      ▼
   Optimizer       Constant-folding pass on the AST
      │
      ▼
   Compiler        AST → Chunk (bytecode + constant pool + line table)
      │
      ├──────────► Serializer (.cvmb binary file)
      │
      ▼
   Virtual Machine Stack-based execution → output
```

---

## Components

### 1. Lexer (`src/lexer.cpp`)

The lexer converts raw source text into a flat stream of tokens. It handles:

- All arithmetic and comparison operators including `%`, `<=`, `>=`, `!=`
- String literals with escape sequences (`\n`, `\t`, `\"`, `\\`)
- Single-line `//` and nested block `/* ... */` comments
- Keywords: `let`, `fn`, `if`, `else`, `while`, `for`, `return`, `print`, `input`, `and`, `or`, `not`, `true`, `false`, `nil`
- Numeric literals pre-parsed to `double`
- Error tokens for unrecognised characters

### 2. Parser (`src/parser.cpp`)

The parser uses **recursive descent** to build a typed AST from the token stream. The grammar handles:

```
expression → assignment → logic_or → logic_and → equality
           → comparison → term → factor → unary → call → primary
```

Key design choices:
- Short-circuit `and`/`or` are parsed as `LogicalExpr` (not `BinaryExpr`) so the compiler can emit conditional jumps.
- Assignment is right-associative: `a = b = c` parses as `a = (b = c)`.
- Error recovery synchronises on statement boundaries so multiple errors can be reported in one pass.
- Parse errors include the source line text with a `^~~~` pointer.

### 3. Abstract Syntax Tree (`src/ast.h`)

The AST uses the **Visitor pattern** — each node type implements `accept(Visitor&)`. This cleanly separates tree structure from tree operations (compilation, printing, optimisation) without `dynamic_cast`.

Node categories:
- **Expressions**: Literal, String, Nil, Identifier, Unary, Binary, Logical, Assign, Call, Input
- **Statements**: ExprStmt, Print, Let, Block, If, While, For, Fn, Return

### 4. Optimizer (`src/optimizer.cpp`)

A pre-compilation tree transformation that evaluates constant sub-expressions at compile time:

| Fold type | Example |
|---|---|
| Number arithmetic | `2 + 3 * 4` → `14` |
| Boolean logic | `!false` → `true` |
| String concatenation | `"foo" + "bar"` → `"foobar"` |
| Comparison | `1 < 2` → `true` |
| Unary negation | `-(5)` → `-5` |

This reduces the number of VM instructions emitted and demonstrates a real compiler optimisation pass.

### 5. Compiler (`src/compiler.cpp`)

The compiler makes a **single pass** over the AST, emitting bytecode directly. Key mechanisms:

**Local variable tracking** — A `locals` vector mirrors the VM stack. When a local variable is declared, it is added to the vector. `resolve_local` walks the vector from back-to-front to find the stack slot. Scope depth is tracked so `end_block` can emit the correct number of `POP` instructions.

**Global variables** — Stored by name in a runtime hash table. The name string is stored as a constant in the chunk and referenced by index.

**Jump backpatching** — Forward jumps emit placeholder bytes `0xFF 0xFF`, then `patch_jump` fills in the real offset once the target is known. Used for `if`, `while`, `for`, and short-circuit operators.

**Functions** — Each `fn` declaration pushes a new `CompilerScope`. The function body is compiled into a fresh `Chunk`. The resulting `Function` object is wrapped in a `Value` and stored as a constant, then defined as a global or local.

**Instruction set** — 30 opcodes covering constants, stack ops, local/global variables, arithmetic, comparisons, control flow, function call/return, and I/O.

### 6. Virtual Machine (`src/vm.cpp`)

The VM is a **call-frame stack machine**:

- **Value stack**: up to 512 values, directly indexed
- **Call frames**: up to 64 frames; each frame stores the function pointer, instruction pointer, and pointer to its base slot in the value stack
- **Globals**: `std::unordered_map<string, Value>` — O(1) average access
- **Locals**: direct slot indexing via `frame.slots[i]` — O(1), no map lookup

The execution loop uses a `switch` over opcodes. Three helper macros (`READ_BYTE`, `READ_SHORT`, `READ_CONST`) keep the hot path tight.

**Native functions** — Standard library functions (`to_num`, `to_str`, `sqrt`, etc.) are implemented as C++ lambdas registered in a `natives_` map. They are dispatched without introducing new opcodes: `GET_GLOBAL` pushes the function name as a sentinel string, and `CALL` checks the `natives_` map before the regular function-call path.

**Profiler** — An optional `Profile` struct counts every opcode execution. After the run, results are sorted by count and printed.

### 7. Disassembler (`src/disassembler.cpp`)

Converts a `Chunk` to human-readable text showing:
- Hex byte offset
- Source line number
- Opcode name, operands, and constant value annotation

Nested function chunks are recursively disassembled.

### 8. Bytecode Serializer (`src/serializer.cpp`)

The `.cvmb` binary format allows a script to be compiled once and run later (or on a different machine with the same CVM++ binary). The format is:

```
magic: "CVM\0"  (4 bytes)
version: u16
arity:   u8
chunk: (recursive)
  name_len u16 + name bytes
  const_cnt u16
  values: type-tagged payloads
  code_cnt u32 + code bytes
  line_cnt u32 + i32 lines
```

Function values inside constants are serialized as embedded chunks.

### 9. REPL (`src/repl.cpp`)

The interactive REPL maintains a persistent `VM` instance across inputs so global variables survive between lines. Toggle commands (`:ast`, `:bytecode`, `:trace`, etc.) let users inspect any evaluation step interactively.

---

## Language Grammar Summary

```
program        → declaration* EOF
declaration    → fnDecl | letDecl | statement
fnDecl         → "fn" IDENTIFIER "(" params? ")" block
letDecl        → "let" IDENTIFIER ("=" expr)? ";"
statement      → exprStmt | printStmt | ifStmt | whileStmt
               | forStmt | returnStmt | block
expression     → assignment | logicOr | logicAnd | equality
               | comparison | term | factor | unary | call | primary
```

---

## What Goes Beyond the Problem Statement

| Extra | Reason it's useful |
|---|---|
| User-defined functions | Enables reusable, recursive code |
| String type | Real programs need text |
| `for` loop | Natural iteration without while boilerplate |
| `%` modulo, `>=`, `<=`, `!=` | Complete operator set |
| Native stdlib (`to_num`, `sqrt`, etc.) | Practical I/O and math |
| `.cvmb` bytecode files | Compile-once / run-anywhere toolchain |
| Constant-folding optimizer | Demonstrates a real compiler pass |
| Execution profiler | Insight into runtime hot paths |
| REPL toggle commands | Pedagogical — inspect any stage live |

---

## Key Concepts Applied

- **Lexical analysis** — character → token with DFA-style state transitions
- **Recursive descent parsing** — grammar rules map directly to functions
- **Visitor pattern** — cleanly decouples tree structure from operations
- **Scope and environment** — compile-time slot allocation vs. runtime map
- **Backpatching** — forward jumps filled in after the target is known
- **Stack machine semantics** — all values flow through an explicit stack
- **Call frames** — function activation records with local variable windows
- **Bytecode serialisation** — binary encoding with type tags and versioning
