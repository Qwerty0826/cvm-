#include "compiler.h"
#include <cmath>

Compiler::Compiler() {}

// ── Scope management ──────────────────────────────────────────────────────────
void Compiler::push_scope(std::shared_ptr<Function> fn) {
    CompilerScope scope;
    scope.function = std::move(fn);
    // Reserve slot 0 for the function itself (for recursive calls)
    scope.locals.push_back({"", 0});
    scopes_.push_back(std::move(scope));
}

std::shared_ptr<Function> Compiler::pop_scope() {
    auto fn = current().function;
    scopes_.pop_back();
    return fn;
}

void Compiler::begin_block() {
    current().scope_depth++;
}

void Compiler::end_block(int line) {
    current().scope_depth--;
    auto& locals = current().locals;
    while (!locals.empty() && locals.back().depth > current().scope_depth) {
        emit(OpCode::POP, line);
        locals.pop_back();
    }
}

// ── Variable resolution ───────────────────────────────────────────────────────
int Compiler::resolve_local(const std::string& name) {
    auto& locals = current().locals;
    for (int i = (int)locals.size() - 1; i >= 0; --i) {
        if (locals[i].name == name) return i;
    }
    return -1; // not found → global
}

void Compiler::declare_local(const std::string& name, int line) {
    if (current().scope_depth == 0) return; // global
    for (int i = (int)current().locals.size() - 1; i >= 0; --i) {
        if (current().locals[i].depth < current().scope_depth) break;
        if (current().locals[i].name == name)
            compile_error(line, "Variable '" + name + "' already declared in this scope.");
    }
    current().locals.push_back({name, current().scope_depth});
}

// ── Emit helpers ──────────────────────────────────────────────────────────────
void Compiler::emit(OpCode op, int line) {
    current_chunk().write_op(op, line);
}

void Compiler::emit(OpCode op, uint8_t operand, int line) {
    current_chunk().write_op(op, line);
    current_chunk().write(operand, line);
}

int Compiler::emit_jump(OpCode op, int line) {
    emit(op, line);
    current_chunk().write(0xFF, line);
    current_chunk().write(0xFF, line);
    return current_chunk().current_offset() - 2;
}

void Compiler::patch_jump(int offset) {
    current_chunk().patch_jump(offset);
}

void Compiler::emit_loop(int loop_start, int line) {
    emit(OpCode::LOOP, line);
    int offset = current_chunk().current_offset() - loop_start + 2;
    if (offset > 0xFFFF) compile_error(line, "Loop body too large.");
    current_chunk().write(static_cast<uint8_t>((offset >> 8) & 0xFF), line);
    current_chunk().write(static_cast<uint8_t>(offset & 0xFF), line);
}

int Compiler::add_string_constant(const std::string& s) {
    return current_chunk().add_constant(Value::from_string(s));
}

void Compiler::compile_error(int line, const std::string& msg) {
    had_error_ = true;
    throw CompileException(CompileError{line, msg});
}

// ── Top-level compilation ─────────────────────────────────────────────────────
std::shared_ptr<Function> Compiler::compile(Program& prog) {
    auto script = std::make_shared<Function>("<script>");
    push_scope(script);

    for (auto& stmt : prog.stmts) {
        try {
            stmt->accept(*this);
        } catch (const CompileException& e) {
            had_error_ = true;
            std::cerr << Color::RED() << "[compile error] line " << e.err.line
                      << ": " << e.err.message << Color::RESET() << "\n";
        }
    }
    emit(OpCode::NIL,  0);
    emit(OpCode::HALT, 0);
    return pop_scope();
}

// ══════════════════════════════════════════════════════════════════════════════
// Expression visitors
// ══════════════════════════════════════════════════════════════════════════════

void Compiler::visit(NilExpr& e) {
    emit(OpCode::NIL, e.line);
}

void Compiler::visit(LiteralExpr& e) {
    if (e.is_bool) {
        emit(e.bool_value ? OpCode::TRUE : OpCode::FALSE, e.line);
    } else {
        current_chunk().write_constant(Value::from_number(e.value), e.line);
    }
}

void Compiler::visit(StringExpr& e) {
    current_chunk().write_constant(Value::from_string(e.value), e.line);
}

void Compiler::visit(IdentifierExpr& e) {
    int local = resolve_local(e.name);
    if (local >= 0) {
        emit(OpCode::GET_LOCAL, static_cast<uint8_t>(local), e.line);
    } else {
        int idx = add_string_constant(e.name);
        emit(OpCode::GET_GLOBAL, e.line);
        current_chunk().write(static_cast<uint8_t>((idx >> 8) & 0xFF), e.line);
        current_chunk().write(static_cast<uint8_t>(idx & 0xFF), e.line);
    }
}

void Compiler::visit(UnaryExpr& e) {
    e.operand->accept(*this);
    switch (e.op) {
        case TokenType::MINUS: emit(OpCode::NEGATE, e.line); break;
        case TokenType::BANG:
        case TokenType::NOT:   emit(OpCode::NOT, e.line);    break;
        default: compile_error(e.line, "Unknown unary operator.");
    }
}

void Compiler::visit(BinaryExpr& e) {
    e.left->accept(*this);
    e.right->accept(*this);
    switch (e.op) {
        case TokenType::PLUS:    emit(OpCode::ADD,           e.line); break;
        case TokenType::MINUS:   emit(OpCode::SUBTRACT,      e.line); break;
        case TokenType::STAR:    emit(OpCode::MULTIPLY,      e.line); break;
        case TokenType::SLASH:   emit(OpCode::DIVIDE,        e.line); break;
        case TokenType::PERCENT: emit(OpCode::MODULO,        e.line); break;
        case TokenType::EQ_EQ:   emit(OpCode::EQUAL,         e.line); break;
        case TokenType::BANG_EQ: emit(OpCode::NOT_EQUAL,     e.line); break;
        case TokenType::LT:      emit(OpCode::LESS,          e.line); break;
        case TokenType::LT_EQ:   emit(OpCode::LESS_EQUAL,    e.line); break;
        case TokenType::GT:      emit(OpCode::GREATER,       e.line); break;
        case TokenType::GT_EQ:   emit(OpCode::GREATER_EQUAL, e.line); break;
        default: compile_error(e.line, "Unknown binary operator.");
    }
}

void Compiler::visit(LogicalExpr& e) {
    // Short-circuit evaluation
    if (e.op == TokenType::AND) {
        e.left->accept(*this);
        int end_jump = emit_jump(OpCode::JUMP_IF_FALSE, e.line);
        emit(OpCode::POP, e.line);
        e.right->accept(*this);
        patch_jump(end_jump);
    } else { // OR
        e.left->accept(*this);
        int else_jump = emit_jump(OpCode::JUMP_IF_FALSE, e.line);
        int end_jump  = emit_jump(OpCode::JUMP, e.line);
        patch_jump(else_jump);
        emit(OpCode::POP, e.line);
        e.right->accept(*this);
        patch_jump(end_jump);
    }
}

void Compiler::visit(AssignExpr& e) {
    e.value->accept(*this);
    int local = resolve_local(e.name);
    if (local >= 0) {
        emit(OpCode::SET_LOCAL, static_cast<uint8_t>(local), e.line);
    } else {
        int idx = add_string_constant(e.name);
        emit(OpCode::SET_GLOBAL, e.line);
        current_chunk().write(static_cast<uint8_t>((idx >> 8) & 0xFF), e.line);
        current_chunk().write(static_cast<uint8_t>(idx & 0xFF), e.line);
    }
}

void Compiler::visit(CallExpr& e) {
    e.callee->accept(*this);
    for (auto& arg : e.args) arg->accept(*this);
    emit(OpCode::CALL, static_cast<uint8_t>(e.args.size()), e.line);
}

void Compiler::visit(InputExpr& e) {
    int idx = add_string_constant(e.prompt);
    emit(OpCode::INPUT, e.line);
    current_chunk().write(static_cast<uint8_t>((idx >> 8) & 0xFF), e.line);
    current_chunk().write(static_cast<uint8_t>(idx & 0xFF), e.line);
}

// ══════════════════════════════════════════════════════════════════════════════
// Statement visitors
// ══════════════════════════════════════════════════════════════════════════════

void Compiler::visit(ExprStmt& s) {
    s.expr->accept(*this);
    emit(OpCode::POP, s.line); // discard expression value
}

void Compiler::visit(PrintStmt& s) {
    s.expr->accept(*this);
    emit(OpCode::PRINT, s.line);
}

void Compiler::visit(LetStmt& s) {
    // Evaluate initializer (or nil)
    if (s.initializer) s.initializer->accept(*this);
    else               emit(OpCode::NIL, s.line);

    if (current().scope_depth > 0) {
        // Local variable: just sits on the stack
        declare_local(s.name, s.line);
    } else {
        // Global variable
        int idx = add_string_constant(s.name);
        emit(OpCode::DEFINE_GLOBAL, s.line);
        current_chunk().write(static_cast<uint8_t>((idx >> 8) & 0xFF), s.line);
        current_chunk().write(static_cast<uint8_t>(idx & 0xFF), s.line);
    }
}

void Compiler::visit(BlockStmt& s) {
    begin_block();
    for (auto& stmt : s.stmts) stmt->accept(*this);
    end_block(s.line);
}

void Compiler::visit(IfStmt& s) {
    // Emits:
    //   condition
    //   JUMP_IF_FALSE → false_label
    //   POP condition              (true path)
    //   then-body
    //   JUMP → end_if              (skip false machinery)
    //   false_label:
    //   POP condition              (false path)
    //   [else-body]
    //   end_if:
    s.condition->accept(*this);
    int then_jump = emit_jump(OpCode::JUMP_IF_FALSE, s.line);
    emit(OpCode::POP, s.line);  // pop condition (true path)
    s.then_branch->accept(*this);

    if (s.else_branch) {
        int else_jump = emit_jump(OpCode::JUMP, s.line);
        patch_jump(then_jump);
        emit(OpCode::POP, s.line); // pop condition (false path)
        s.else_branch->accept(*this);
        patch_jump(else_jump);
    } else {
        // Jump over the false-path POP so true path doesn't execute it.
        int end_jump = emit_jump(OpCode::JUMP, s.line);
        patch_jump(then_jump);
        emit(OpCode::POP, s.line); // pop condition (false path)
        patch_jump(end_jump);
    }
}

void Compiler::visit(WhileStmt& s) {
    int loop_start = current_chunk().current_offset();
    s.condition->accept(*this);
    int exit_jump = emit_jump(OpCode::JUMP_IF_FALSE, s.line);
    emit(OpCode::POP, s.line);
    s.body->accept(*this);
    emit_loop(loop_start, s.line);
    patch_jump(exit_jump);
    emit(OpCode::POP, s.line);
}

void Compiler::visit(ForStmt& s) {
    begin_block(); // for-init variable scoped to the loop
    if (s.init) s.init->accept(*this);

    int loop_start = current_chunk().current_offset();
    int exit_jump  = -1;
    if (s.condition) {
        s.condition->accept(*this);
        exit_jump = emit_jump(OpCode::JUMP_IF_FALSE, s.line);
        emit(OpCode::POP, s.line);
    }

    s.body->accept(*this);

    if (s.post) {
        s.post->accept(*this);
        emit(OpCode::POP, s.line); // discard post-expression result
    }

    emit_loop(loop_start, s.line);

    if (exit_jump >= 0) {
        patch_jump(exit_jump);
        emit(OpCode::POP, s.line);
    }
    end_block(s.line);
}

void Compiler::visit(FnStmt& s) {
    // Compile the function body into its own chunk.
    auto fn = std::make_shared<Function>(s.name);
    fn->arity = static_cast<int>(s.params.size());

    push_scope(fn);
    begin_block();

    // Bind parameters as locals (slot 0 is the function itself).
    for (auto& p : s.params) declare_local(p, s.line);

    for (auto& stmt : s.body) stmt->accept(*this);

    // Implicit nil return
    emit(OpCode::NIL,    s.line);
    emit(OpCode::RETURN, s.line);

    end_block(s.line);
    auto compiled_fn = pop_scope();

    // Define the function in the enclosing scope.
    if (current().scope_depth > 0) {
        // Local function: push it onto the stack as a local variable.
        current_chunk().write_constant(Value::from_function(compiled_fn), s.line);
        declare_local(s.name, s.line);
    } else {
        // Global function
        current_chunk().write_constant(Value::from_function(compiled_fn), s.line);
        int idx = add_string_constant(s.name);
        emit(OpCode::DEFINE_GLOBAL, s.line);
        current_chunk().write(static_cast<uint8_t>((idx >> 8) & 0xFF), s.line);
        current_chunk().write(static_cast<uint8_t>(idx & 0xFF), s.line);
    }
}

void Compiler::visit(ReturnStmt& s) {
    if (scopes_.size() == 1)
        compile_error(s.line, "Cannot return from top-level code.");
    if (s.value) s.value->accept(*this);
    else         emit(OpCode::NIL, s.line);
    emit(OpCode::RETURN, s.line);
}
