#include "optimizer.h"
#include <cmath>

// ── Helpers ───────────────────────────────────────────────────────────────────

static bool is_number_literal(const Expr* e, double& out) {
    auto* lit = dynamic_cast<const LiteralExpr*>(e);
    if (lit && !lit->is_bool) { out = lit->value; return true; }
    return false;
}

static bool is_bool_literal(const Expr* e, bool& out) {
    auto* lit = dynamic_cast<const LiteralExpr*>(e);
    if (lit && lit->is_bool) { out = lit->bool_value; return true; }
    return false;
}

static bool is_string_literal(const Expr* e, std::string& out) {
    auto* s = dynamic_cast<const StringExpr*>(e);
    if (s) { out = s->value; return true; }
    return false;
}

// ── Fold expressions ──────────────────────────────────────────────────────────
ExprPtr Optimizer::fold_expr(ExprPtr& expr) {
    if (!expr) return nullptr;

    // ── Recursively fold children first ──────────────────────────────
    if (auto* bin = dynamic_cast<BinaryExpr*>(expr.get())) {
        if (auto folded = fold_expr(bin->left))  bin->left  = std::move(folded);
        if (auto folded = fold_expr(bin->right)) bin->right = std::move(folded);

        double la, ra;
        std::string ls, rs;
        bool lb, rb;
        int ln = bin->line;

        // Number op Number
        if (is_number_literal(bin->left.get(), la) && is_number_literal(bin->right.get(), ra)) {
            double result = 0;
            bool   is_bool_result = false;
            bool   bool_result    = false;

            switch (bin->op) {
                case TokenType::PLUS:    result = la + ra; break;
                case TokenType::MINUS:   result = la - ra; break;
                case TokenType::STAR:    result = la * ra; break;
                case TokenType::SLASH:
                    if (ra == 0) goto no_fold; // don't fold div-by-zero
                    result = la / ra; break;
                case TokenType::PERCENT:
                    if (ra == 0) goto no_fold;
                    result = std::fmod(la, ra); break;
                case TokenType::EQ_EQ:   is_bool_result = true; bool_result = la == ra; break;
                case TokenType::BANG_EQ: is_bool_result = true; bool_result = la != ra; break;
                case TokenType::LT:      is_bool_result = true; bool_result = la <  ra; break;
                case TokenType::LT_EQ:   is_bool_result = true; bool_result = la <= ra; break;
                case TokenType::GT:      is_bool_result = true; bool_result = la >  ra; break;
                case TokenType::GT_EQ:   is_bool_result = true; bool_result = la >= ra; break;
                default: goto no_fold;
            }
            folds_++;
            if (is_bool_result) return std::make_unique<LiteralExpr>(bool_result, ln);
            return std::make_unique<LiteralExpr>(result, ln);
        }

        // String + String
        if (bin->op == TokenType::PLUS &&
            is_string_literal(bin->left.get(), ls) && is_string_literal(bin->right.get(), rs)) {
            folds_++;
            return std::make_unique<StringExpr>(ls + rs, ln);
        }

        // Boolean == / !=
        if ((bin->op == TokenType::EQ_EQ || bin->op == TokenType::BANG_EQ) &&
            is_bool_literal(bin->left.get(), lb) && is_bool_literal(bin->right.get(), rb)) {
            folds_++;
            bool r = (bin->op == TokenType::EQ_EQ) ? (lb == rb) : (lb != rb);
            return std::make_unique<LiteralExpr>(r, ln);
        }
    }

    if (auto* un = dynamic_cast<UnaryExpr*>(expr.get())) {
        if (auto folded = fold_expr(un->operand)) un->operand = std::move(folded);
        double n; bool b;
        if ((un->op == TokenType::MINUS) && is_number_literal(un->operand.get(), n)) {
            folds_++;
            return std::make_unique<LiteralExpr>(-n, un->line);
        }
        if ((un->op == TokenType::BANG || un->op == TokenType::NOT) &&
            is_bool_literal(un->operand.get(), b)) {
            folds_++;
            return std::make_unique<LiteralExpr>(!b, un->line);
        }
    }

    // Recurse into other expression types' subexpressions
    if (auto* call = dynamic_cast<CallExpr*>(expr.get())) {
        if (auto folded = fold_expr(call->callee)) call->callee = std::move(folded);
        for (auto& arg : call->args)
            if (auto folded = fold_expr(arg)) arg = std::move(folded);
    }
    if (auto* assign = dynamic_cast<AssignExpr*>(expr.get())) {
        if (auto folded = fold_expr(assign->value)) assign->value = std::move(folded);
    }
    if (auto* logical = dynamic_cast<LogicalExpr*>(expr.get())) {
        if (auto folded = fold_expr(logical->left))  logical->left  = std::move(folded);
        if (auto folded = fold_expr(logical->right)) logical->right = std::move(folded);
    }

    no_fold:
    return nullptr;
}

// ── Fold statements ───────────────────────────────────────────────────────────
void Optimizer::fold_stmt(StmtPtr& stmt) {
    if (!stmt) return;

    if (auto* s = dynamic_cast<ExprStmt*>(stmt.get())) {
        if (auto f = fold_expr(s->expr)) s->expr = std::move(f);
    } else if (auto* s = dynamic_cast<PrintStmt*>(stmt.get())) {
        if (auto f = fold_expr(s->expr)) s->expr = std::move(f);
    } else if (auto* s = dynamic_cast<LetStmt*>(stmt.get())) {
        if (s->initializer)
            if (auto f = fold_expr(s->initializer)) s->initializer = std::move(f);
    } else if (auto* s = dynamic_cast<BlockStmt*>(stmt.get())) {
        for (auto& child : s->stmts) fold_stmt(child);
    } else if (auto* s = dynamic_cast<IfStmt*>(stmt.get())) {
        if (auto f = fold_expr(s->condition)) s->condition = std::move(f);
        fold_stmt(s->then_branch);
        fold_stmt(s->else_branch);
    } else if (auto* s = dynamic_cast<WhileStmt*>(stmt.get())) {
        if (auto f = fold_expr(s->condition)) s->condition = std::move(f);
        fold_stmt(s->body);
    } else if (auto* s = dynamic_cast<ForStmt*>(stmt.get())) {
        fold_stmt(s->init);
        if (s->condition) if (auto f = fold_expr(s->condition)) s->condition = std::move(f);
        if (s->post)      if (auto f = fold_expr(s->post))      s->post      = std::move(f);
        fold_stmt(s->body);
    } else if (auto* s = dynamic_cast<FnStmt*>(stmt.get())) {
        for (auto& child : s->body) fold_stmt(child);
    } else if (auto* s = dynamic_cast<ReturnStmt*>(stmt.get())) {
        if (s->value) if (auto f = fold_expr(s->value)) s->value = std::move(f);
    }
}

int Optimizer::fold(Program& prog) {
    folds_ = 0;
    for (auto& stmt : prog.stmts) fold_stmt(stmt);
    return folds_;
}
