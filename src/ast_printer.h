#pragma once
#include "ast.h"
#include <ostream>

// Pretty-prints the AST as an indented tree for debug/educational display.
class AstPrinter : public ExprVisitor, public StmtVisitor {
public:
    explicit AstPrinter(std::ostream& out) : out_(out) {}

    void print(const Program& prog);

private:
    std::ostream& out_;
    int           indent_ = 0;

    void line(const std::string& s);
    void indent_in()  { indent_ += 2; }
    void indent_out() { indent_ -= 2; }

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
};
