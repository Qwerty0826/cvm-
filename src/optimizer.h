#pragma once
#include "ast.h"

// ── Constant-Folding Optimizer ────────────────────────────────────────────────
// A single-pass tree transformation that evaluates constant subexpressions
// at compile time, reducing the number of runtime instructions.
//
// Examples:
//   2 + 3        → 5
//   !false       → true
//   "hi" + "!"   → "hi!"
//   1 < 2        → true
class Optimizer {
public:
    // Transform the program in-place.  Returns the number of folds applied.
    int fold(Program& prog);

private:
    int folds_ = 0;

    // Returns a new (folded) ExprPtr, or nullptr if no folding was possible.
    ExprPtr fold_expr(ExprPtr& expr);
    void    fold_stmt(StmtPtr& stmt);
};
