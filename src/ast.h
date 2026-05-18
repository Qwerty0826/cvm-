#pragma once
#include "common.h"
#include "lexer.h"

// ── AST node hierarchy ────────────────────────────────────────────────────────
// We use a classic tagged-union / visitor pattern.  Every node carries a line
// number so the compiler can attach accurate debug info to bytecode.

// Forward declarations for Stmt
struct ExprStmt;
struct PrintStmt;
struct LetStmt;
struct BlockStmt;
struct IfStmt;
struct WhileStmt;
struct ForStmt;
struct FnStmt;
struct ReturnStmt;

// Forward declarations for Expr
struct LiteralExpr;
struct StringExpr;
struct NilExpr;
struct IdentifierExpr;
struct UnaryExpr;
struct BinaryExpr;
struct LogicalExpr;
struct AssignExpr;
struct CallExpr;
struct InputExpr;

// ── Expr visitor ─────────────────────────────────────────────────────────────
struct ExprVisitor {
    virtual ~ExprVisitor() = default;
    virtual void visit(LiteralExpr&)    = 0;
    virtual void visit(StringExpr&)     = 0;
    virtual void visit(NilExpr&)        = 0;
    virtual void visit(IdentifierExpr&) = 0;
    virtual void visit(UnaryExpr&)      = 0;
    virtual void visit(BinaryExpr&)     = 0;
    virtual void visit(LogicalExpr&)    = 0;
    virtual void visit(AssignExpr&)     = 0;
    virtual void visit(CallExpr&)       = 0;
    virtual void visit(InputExpr&)      = 0;
};

struct Expr {
    int line = 0;
    virtual ~Expr() = default;
    virtual void accept(ExprVisitor&) = 0;
};

using ExprPtr = std::unique_ptr<Expr>;

struct LiteralExpr : Expr {
    double value;
    bool   is_bool;
    bool   bool_value;
    explicit LiteralExpr(double v, int ln)       : value(v), is_bool(false), bool_value(false) { line = ln; }
    explicit LiteralExpr(bool b, int ln)         : value(0), is_bool(true),  bool_value(b)     { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct StringExpr : Expr {
    std::string value;
    explicit StringExpr(std::string s, int ln) : value(std::move(s)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct NilExpr : Expr {
    explicit NilExpr(int ln) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct IdentifierExpr : Expr {
    std::string name;
    explicit IdentifierExpr(std::string n, int ln) : name(std::move(n)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct UnaryExpr : Expr {
    TokenType op;
    ExprPtr   operand;
    UnaryExpr(TokenType op, ExprPtr operand, int ln)
        : op(op), operand(std::move(operand)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct BinaryExpr : Expr {
    TokenType op;
    ExprPtr   left;
    ExprPtr   right;
    BinaryExpr(TokenType op, ExprPtr left, ExprPtr right, int ln)
        : op(op), left(std::move(left)), right(std::move(right)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct LogicalExpr : Expr {
    TokenType op;   // AND or OR
    ExprPtr   left;
    ExprPtr   right;
    LogicalExpr(TokenType op, ExprPtr left, ExprPtr right, int ln)
        : op(op), left(std::move(left)), right(std::move(right)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct AssignExpr : Expr {
    std::string name;
    ExprPtr     value;
    AssignExpr(std::string name, ExprPtr value, int ln)
        : name(std::move(name)), value(std::move(value)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct CallExpr : Expr {
    ExprPtr                   callee;
    std::vector<ExprPtr>      args;
    CallExpr(ExprPtr callee, std::vector<ExprPtr> args, int ln)
        : callee(std::move(callee)), args(std::move(args)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

struct InputExpr : Expr {
    std::string prompt;
    explicit InputExpr(std::string prompt, int ln) : prompt(std::move(prompt)) { line = ln; }
    void accept(ExprVisitor& v) override { v.visit(*this); }
};

// ── Stmt visitor ──────────────────────────────────────────────────────────────
struct StmtVisitor {
    virtual ~StmtVisitor() = default;
    virtual void visit(ExprStmt&)   = 0;
    virtual void visit(PrintStmt&)  = 0;
    virtual void visit(LetStmt&)    = 0;
    virtual void visit(BlockStmt&)  = 0;
    virtual void visit(IfStmt&)     = 0;
    virtual void visit(WhileStmt&)  = 0;
    virtual void visit(ForStmt&)    = 0;
    virtual void visit(FnStmt&)     = 0;
    virtual void visit(ReturnStmt&) = 0;
};

struct Stmt {
    int line = 0;
    virtual ~Stmt() = default;
    virtual void accept(StmtVisitor&) = 0;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt : Stmt {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e, int ln) : expr(std::move(e)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

struct PrintStmt : Stmt {
    ExprPtr expr;
    explicit PrintStmt(ExprPtr e, int ln) : expr(std::move(e)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

struct LetStmt : Stmt {
    std::string name;
    ExprPtr     initializer; // may be null (→ nil)
    LetStmt(std::string name, ExprPtr init, int ln)
        : name(std::move(name)), initializer(std::move(init)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

struct BlockStmt : Stmt {
    std::vector<StmtPtr> stmts;
    explicit BlockStmt(std::vector<StmtPtr> stmts, int ln)
        : stmts(std::move(stmts)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

struct IfStmt : Stmt {
    ExprPtr  condition;
    StmtPtr  then_branch;
    StmtPtr  else_branch; // may be null
    IfStmt(ExprPtr cond, StmtPtr then_br, StmtPtr else_br, int ln)
        : condition(std::move(cond)),
          then_branch(std::move(then_br)),
          else_branch(std::move(else_br)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

struct WhileStmt : Stmt {
    ExprPtr condition;
    StmtPtr body;
    WhileStmt(ExprPtr cond, StmtPtr body, int ln)
        : condition(std::move(cond)), body(std::move(body)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

// for (init ; cond ; post) body
struct ForStmt : Stmt {
    StmtPtr init;      // may be null
    ExprPtr condition; // may be null (infinite)
    ExprPtr post;      // may be null
    StmtPtr body;
    ForStmt(StmtPtr init, ExprPtr cond, ExprPtr post, StmtPtr body, int ln)
        : init(std::move(init)), condition(std::move(cond)),
          post(std::move(post)), body(std::move(body)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

struct FnStmt : Stmt {
    std::string              name;
    std::vector<std::string> params;
    std::vector<StmtPtr>     body;
    FnStmt(std::string name, std::vector<std::string> params,
           std::vector<StmtPtr> body, int ln)
        : name(std::move(name)), params(std::move(params)),
          body(std::move(body)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

struct ReturnStmt : Stmt {
    ExprPtr value; // may be null → return nil
    explicit ReturnStmt(ExprPtr val, int ln) : value(std::move(val)) { line = ln; }
    void accept(StmtVisitor& v) override { v.visit(*this); }
};

// ── Top-level program ─────────────────────────────────────────────────────────
struct Program {
    std::vector<StmtPtr> stmts;
};
