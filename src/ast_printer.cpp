#include "ast_printer.h"
#include <sstream>

static const char* op_str(TokenType t) {
    switch (t) {
        case TokenType::PLUS:    return "+";
        case TokenType::MINUS:   return "-";
        case TokenType::STAR:    return "*";
        case TokenType::SLASH:   return "/";
        case TokenType::PERCENT: return "%";
        case TokenType::EQ_EQ:   return "==";
        case TokenType::BANG_EQ: return "!=";
        case TokenType::LT:      return "<";
        case TokenType::LT_EQ:   return "<=";
        case TokenType::GT:      return ">";
        case TokenType::GT_EQ:   return ">=";
        case TokenType::AND:     return "and";
        case TokenType::OR:      return "or";
        case TokenType::BANG:
        case TokenType::NOT:     return "not";
        default:                 return "?";
    }
}

void AstPrinter::line(const std::string& s) {
    out_ << Color::CYAN();
    out_ << std::string(indent_, ' ');
    out_ << Color::RESET() << s << "\n";
}

void AstPrinter::print(const Program& prog) {
    out_ << Color::BOLD() << Color::CYAN()
         << "══ AST ══\n" << Color::RESET();
    for (auto& stmt : prog.stmts) {
        const_cast<Stmt&>(*stmt).accept(*this);
    }
    out_ << "\n";
}

// ── Expressions ───────────────────────────────────────────────────────────────
void AstPrinter::visit(LiteralExpr& e) {
    if (e.is_bool) line(e.bool_value ? "Literal(true)" : "Literal(false)");
    else           line("Literal(" + std::to_string(e.value) + ")");
}
void AstPrinter::visit(StringExpr& e) {
    line("String(\"" + e.value + "\")");
}
void AstPrinter::visit(NilExpr&) {
    line("Nil");
}
void AstPrinter::visit(IdentifierExpr& e) {
    line("Identifier(" + e.name + ")");
}
void AstPrinter::visit(UnaryExpr& e) {
    line(std::string("Unary(") + op_str(e.op) + ")");
    indent_in(); e.operand->accept(*this); indent_out();
}
void AstPrinter::visit(BinaryExpr& e) {
    line(std::string("Binary(") + op_str(e.op) + ")");
    indent_in();
    e.left->accept(*this);
    e.right->accept(*this);
    indent_out();
}
void AstPrinter::visit(LogicalExpr& e) {
    line(std::string("Logical(") + op_str(e.op) + ")");
    indent_in();
    e.left->accept(*this);
    e.right->accept(*this);
    indent_out();
}
void AstPrinter::visit(AssignExpr& e) {
    line("Assign(" + e.name + ")");
    indent_in(); e.value->accept(*this); indent_out();
}
void AstPrinter::visit(CallExpr& e) {
    line("Call(" + std::to_string(e.args.size()) + " args)");
    indent_in();
    e.callee->accept(*this);
    for (auto& arg : e.args) arg->accept(*this);
    indent_out();
}
void AstPrinter::visit(InputExpr& e) {
    line("Input(\"" + e.prompt + "\")");
}

// ── Statements ────────────────────────────────────────────────────────────────
void AstPrinter::visit(ExprStmt& s) {
    line("ExprStmt");
    indent_in(); s.expr->accept(*this); indent_out();
}
void AstPrinter::visit(PrintStmt& s) {
    line("PrintStmt");
    indent_in(); s.expr->accept(*this); indent_out();
}
void AstPrinter::visit(LetStmt& s) {
    line("Let(" + s.name + ")");
    if (s.initializer) {
        indent_in(); s.initializer->accept(*this); indent_out();
    }
}
void AstPrinter::visit(BlockStmt& s) {
    line("Block");
    indent_in();
    for (auto& child : s.stmts) child->accept(*this);
    indent_out();
}
void AstPrinter::visit(IfStmt& s) {
    line("If");
    indent_in();
    line("Condition:"); indent_in(); s.condition->accept(*this); indent_out();
    line("Then:");      indent_in(); s.then_branch->accept(*this); indent_out();
    if (s.else_branch) { line("Else:"); indent_in(); s.else_branch->accept(*this); indent_out(); }
    indent_out();
}
void AstPrinter::visit(WhileStmt& s) {
    line("While");
    indent_in();
    line("Condition:"); indent_in(); s.condition->accept(*this); indent_out();
    line("Body:");      indent_in(); s.body->accept(*this); indent_out();
    indent_out();
}
void AstPrinter::visit(ForStmt& s) {
    line("For");
    indent_in();
    if (s.init)      { line("Init:");      indent_in(); s.init->accept(*this); indent_out(); }
    if (s.condition) { line("Condition:"); indent_in(); s.condition->accept(*this); indent_out(); }
    if (s.post)      { line("Post:");      indent_in(); s.post->accept(*this); indent_out(); }
    line("Body:"); indent_in(); s.body->accept(*this); indent_out();
    indent_out();
}
void AstPrinter::visit(FnStmt& s) {
    std::string params;
    for (size_t i = 0; i < s.params.size(); ++i) {
        params += s.params[i];
        if (i+1 < s.params.size()) params += ", ";
    }
    line("Fn(" + s.name + ")  params: [" + params + "]");
    indent_in();
    for (auto& child : s.body) child->accept(*this);
    indent_out();
}
void AstPrinter::visit(ReturnStmt& s) {
    line("Return");
    if (s.value) { indent_in(); s.value->accept(*this); indent_out(); }
}
