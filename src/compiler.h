#pragma once
#include "common.h"
#include "ast.h"
#include "chunk.h"

struct CompileError {
    int         line;
    std::string message;
};

class CompileException : public std::exception {
public:
    CompileError err;
    explicit CompileException(CompileError e) : err(std::move(e)) {}
    const char* what() const noexcept override { return err.message.c_str(); }
};

// ── Compiler ──────────────────────────────────────────────────────────────────
// Walks the AST and emits bytecode into Chunk objects.
// Uses a simple single-pass scope tracker for locals.
class Compiler : public ExprVisitor, public StmtVisitor {
public:
    Compiler();

    // Compile a parsed program into the top-level function object.
    std::shared_ptr<Function> compile(Program& prog);

    bool had_error() const { return had_error_; }

private:
    // ── Scope / local variable tracking ──────────────────────────────
    struct Local {
        std::string name;
        int         depth;
    };

    struct CompilerScope {
        std::shared_ptr<Function>  function;
        std::vector<Local>         locals;
        int                        scope_depth = 0;
    };

    std::vector<CompilerScope> scopes_;   // stack; back() = current
    bool                       had_error_ = false;

    // Convenience accessors
    CompilerScope& current()        { return scopes_.back(); }
    Chunk&         current_chunk()  { return *current().function->chunk; }

    // ── Scope management ──────────────────────────────────────────────
    void push_scope(std::shared_ptr<Function> fn);
    std::shared_ptr<Function> pop_scope();
    void begin_block();
    void end_block(int line);

    // ── Variable resolution ───────────────────────────────────────────
    int  resolve_local(const std::string& name);
    void declare_local(const std::string& name, int line);
    void define_local();

    // ── Emit helpers ──────────────────────────────────────────────────
    void emit(OpCode op, int line);
    void emit(OpCode op, uint8_t operand, int line);
    int  emit_jump(OpCode op, int line);
    void patch_jump(int offset);
    void emit_loop(int loop_start, int line);
    int  add_string_constant(const std::string& s, int line);

    // ── Visitor implementations ───────────────────────────────────────
    void visit(LiteralExpr&)    override;
    void visit(StringExpr&)     override;
    void visit(NilExpr&)        override;
    void visit(IdentifierExpr&) override;
    void visit(UnaryExpr&)      override;
    void visit(BinaryExpr&)     override;
    void visit(LogicalExpr&)    override;
    void visit(AssignExpr&)     override;
    void visit(CallExpr&)       override;
    void visit(InputExpr&)      override;

    void visit(ExprStmt&)   override;
    void visit(PrintStmt&)  override;
    void visit(LetStmt&)    override;
    void visit(BlockStmt&)  override;
    void visit(IfStmt&)     override;
    void visit(WhileStmt&)  override;
    void visit(ForStmt&)    override;
    void visit(FnStmt&)     override;
    void visit(ReturnStmt&) override;

    void compile_error(int line, const std::string& msg);
};
