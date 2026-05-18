#include "parser.h"
#include <sstream>

Parser::Parser(std::vector<Token> tokens, const std::string& source)
    : tokens_(std::move(tokens)), source_(source) {}

// ── Token helpers ─────────────────────────────────────────────────────────────
Token& Parser::peek()     { return tokens_[current_]; }
Token& Parser::previous() { return tokens_[current_ - 1]; }
bool   Parser::is_at_end(){ return peek().type == TokenType::EOF_TOK; }

Token Parser::advance() {
    if (!is_at_end()) current_++;
    return previous();
}
bool Parser::check(TokenType t) {
    return !is_at_end() && peek().type == t;
}
bool Parser::match(std::initializer_list<TokenType> types) {
    for (auto t : types) { if (check(t)) { advance(); return true; } }
    return false;
}
Token Parser::consume(TokenType t, const std::string& msg) {
    if (check(t)) return advance();
    throw error(peek(), msg);
}

std::string Parser::source_line(int line) const {
    int cur_line = 1, start = 0;
    for (int i = 0; i < (int)source_.size(); ++i) {
        if (cur_line == line) {
            // find end
            int end = i;
            while (end < (int)source_.size() && source_[end] != '\n') end++;
            return source_.substr(i, end - i);
        }
        if (source_[i] == '\n') { cur_line++; start = i + 1; }
        else { (void)start; }
    }
    return "";
}

ParseException Parser::error(const Token& tok, const std::string& msg) {
    ParseError e;
    e.line    = tok.line;
    e.message = msg;
    e.context = source_line(tok.line);
    errors_.push_back(e);
    return ParseException(e);
}

void Parser::synchronise() {
    advance();
    while (!is_at_end()) {
        if (previous().type == TokenType::SEMICOLON) return;
        switch (peek().type) {
            case TokenType::FN:
            case TokenType::LET:
            case TokenType::IF:
            case TokenType::WHILE:
            case TokenType::FOR:
            case TokenType::RETURN:
            case TokenType::PRINT:
                return;
            default: break;
        }
        advance();
    }
}

// ── Top-level ─────────────────────────────────────────────────────────────────
Program Parser::parse() {
    Program prog;
    while (!is_at_end()) {
        try {
            prog.stmts.push_back(declaration());
        } catch (const ParseException&) {
            synchronise();
        }
    }
    return prog;
}

// ── Declarations ─────────────────────────────────────────────────────────────
StmtPtr Parser::declaration() {
    if (match({TokenType::FN}))  return fn_declaration();
    if (match({TokenType::LET})) return let_declaration();
    return statement();
}

StmtPtr Parser::fn_declaration() {
    int ln = previous().line;
    Token name = consume(TokenType::IDENTIFIER, "Expected function name.");
    consume(TokenType::LPAREN, "Expected '(' after function name.");
    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        do {
            if (params.size() >= 255)
                throw error(peek(), "Cannot have more than 255 parameters.");
            params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name.").lexeme);
        } while (match({TokenType::COMMA}));
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters.");
    consume(TokenType::LBRACE, "Expected '{' before function body.");

    std::vector<StmtPtr> body;
    while (!check(TokenType::RBRACE) && !is_at_end())
        body.push_back(declaration());
    consume(TokenType::RBRACE, "Expected '}' after function body.");

    return std::make_unique<FnStmt>(name.lexeme, std::move(params), std::move(body), ln);
}

StmtPtr Parser::let_declaration() {
    int ln = previous().line;
    Token name = consume(TokenType::IDENTIFIER, "Expected variable name.");
    ExprPtr init;
    if (match({TokenType::EQ})) init = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration.");
    return std::make_unique<LetStmt>(name.lexeme, std::move(init), ln);
}

// ── Statements ────────────────────────────────────────────────────────────────
StmtPtr Parser::statement() {
    if (match({TokenType::PRINT}))  return print_statement();
    if (match({TokenType::RETURN})) return return_statement();
    if (match({TokenType::IF}))     return if_statement();
    if (match({TokenType::WHILE}))  return while_statement();
    if (match({TokenType::FOR}))    return for_statement();
    if (match({TokenType::LBRACE})) return block(previous().line);
    return expr_statement();
}

StmtPtr Parser::print_statement() {
    int ln = previous().line;
    ExprPtr val = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after print value.");
    return std::make_unique<PrintStmt>(std::move(val), ln);
}

StmtPtr Parser::return_statement() {
    int ln = previous().line;
    ExprPtr val;
    if (!check(TokenType::SEMICOLON)) val = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after return value.");
    return std::make_unique<ReturnStmt>(std::move(val), ln);
}

StmtPtr Parser::if_statement() {
    int ln = previous().line;
    consume(TokenType::LPAREN, "Expected '(' after 'if'.");
    ExprPtr cond = expression();
    consume(TokenType::RPAREN, "Expected ')' after if condition.");

    StmtPtr then_br = statement();
    StmtPtr else_br;
    if (match({TokenType::ELSE})) else_br = statement();

    return std::make_unique<IfStmt>(std::move(cond), std::move(then_br), std::move(else_br), ln);
}

StmtPtr Parser::while_statement() {
    int ln = previous().line;
    consume(TokenType::LPAREN, "Expected '(' after 'while'.");
    ExprPtr cond = expression();
    consume(TokenType::RPAREN, "Expected ')' after while condition.");
    StmtPtr body = statement();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), ln);
}

StmtPtr Parser::for_statement() {
    int ln = previous().line;
    consume(TokenType::LPAREN, "Expected '(' after 'for'.");

    StmtPtr init;
    if (match({TokenType::SEMICOLON})) { /* no init */ }
    else if (match({TokenType::LET}))  init = let_declaration();
    else                               init = expr_statement();

    ExprPtr cond;
    if (!check(TokenType::SEMICOLON)) cond = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after for condition.");

    ExprPtr post;
    if (!check(TokenType::RPAREN)) post = expression();
    consume(TokenType::RPAREN, "Expected ')' after for clauses.");

    StmtPtr body = statement();
    return std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(post), std::move(body), ln);
}

StmtPtr Parser::expr_statement() {
    int ln = peek().line;
    ExprPtr e = expression();
    consume(TokenType::SEMICOLON, "Expected ';' after expression.");
    return std::make_unique<ExprStmt>(std::move(e), ln);
}

StmtPtr Parser::block(int ln) {
    std::vector<StmtPtr> stmts;
    while (!check(TokenType::RBRACE) && !is_at_end())
        stmts.push_back(declaration());
    consume(TokenType::RBRACE, "Expected '}' after block.");
    return std::make_unique<BlockStmt>(std::move(stmts), ln);
}

// ── Expressions ───────────────────────────────────────────────────────────────
ExprPtr Parser::expression() { return assignment(); }

ExprPtr Parser::assignment() {
    ExprPtr expr = logic_or();
    if (match({TokenType::EQ})) {
        int ln = previous().line;
        ExprPtr value = assignment(); // right-associative
        if (auto* id = dynamic_cast<IdentifierExpr*>(expr.get())) {
            return std::make_unique<AssignExpr>(id->name, std::move(value), ln);
        }
        throw error(previous(), "Invalid assignment target.");
    }
    return expr;
}

ExprPtr Parser::logic_or() {
    ExprPtr left = logic_and();
    while (match({TokenType::OR})) {
        int ln = previous().line;
        ExprPtr right = logic_and();
        left = std::make_unique<LogicalExpr>(TokenType::OR, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::logic_and() {
    ExprPtr left = equality();
    while (match({TokenType::AND})) {
        int ln = previous().line;
        ExprPtr right = equality();
        left = std::make_unique<LogicalExpr>(TokenType::AND, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::equality() {
    ExprPtr left = comparison();
    while (match({TokenType::EQ_EQ, TokenType::BANG_EQ})) {
        TokenType op = previous().type;
        int ln = previous().line;
        ExprPtr right = comparison();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::comparison() {
    ExprPtr left = term();
    while (match({TokenType::GT, TokenType::GT_EQ, TokenType::LT, TokenType::LT_EQ})) {
        TokenType op = previous().type;
        int ln = previous().line;
        ExprPtr right = term();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::term() {
    ExprPtr left = factor();
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        TokenType op = previous().type;
        int ln = previous().line;
        ExprPtr right = factor();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::factor() {
    ExprPtr left = unary();
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        TokenType op = previous().type;
        int ln = previous().line;
        ExprPtr right = unary();
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::unary() {
    if (match({TokenType::BANG, TokenType::MINUS, TokenType::NOT})) {
        TokenType op = previous().type;
        int ln = previous().line;
        ExprPtr operand = unary();
        return std::make_unique<UnaryExpr>(op, std::move(operand), ln);
    }
    return call();
}

ExprPtr Parser::call() {
    ExprPtr expr = primary();
    while (true) {
        if (match({TokenType::LPAREN})) {
            int ln = previous().line;
            std::vector<ExprPtr> args;
            if (!check(TokenType::RPAREN)) {
                do {
                    if (args.size() >= 255)
                        throw error(peek(), "Cannot have more than 255 arguments.");
                    args.push_back(expression());
                } while (match({TokenType::COMMA}));
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments.");
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args), ln);
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::primary() {
    if (match({TokenType::TRUE_KW}))
        return std::make_unique<LiteralExpr>(true, previous().line);
    if (match({TokenType::FALSE_KW}))
        return std::make_unique<LiteralExpr>(false, previous().line);
    if (match({TokenType::NIL_KW}))
        return std::make_unique<NilExpr>(previous().line);
    if (match({TokenType::NUMBER}))
        return std::make_unique<LiteralExpr>(previous().num_value, previous().line);
    if (match({TokenType::STRING}))
        return std::make_unique<StringExpr>(previous().lexeme, previous().line);
    if (match({TokenType::IDENTIFIER}))
        return std::make_unique<IdentifierExpr>(previous().lexeme, previous().line);

    // input("prompt") or input()
    if (match({TokenType::INPUT})) {
        int ln = previous().line;
        consume(TokenType::LPAREN, "Expected '(' after 'input'.");
        std::string prompt;
        if (check(TokenType::STRING)) {
            prompt = advance().lexeme;
        }
        consume(TokenType::RPAREN, "Expected ')' after input prompt.");
        return std::make_unique<InputExpr>(std::move(prompt), ln);
    }

    if (match({TokenType::LPAREN})) {
        int ln = previous().line;
        ExprPtr e = expression();
        consume(TokenType::RPAREN, "Expected ')' after expression.");
        (void)ln;
        return e;
    }

    throw error(peek(), "Expected expression.");
}
