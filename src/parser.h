#pragma once
#include "common.h"
#include "lexer.h"
#include "ast.h"

// ── Parse error with source-context ──────────────────────────────────────────
struct ParseError {
    int         line;
    std::string message;
    std::string context; // the offending line of source text
};

class ParseException : public std::exception {
public:
    ParseError err;
    explicit ParseException(ParseError e) : err(std::move(e)) {}
    const char* what() const noexcept override { return err.message.c_str(); }
};

// ── Parser ────────────────────────────────────────────────────────────────────
class Parser {
public:
    Parser(std::vector<Token> tokens, const std::string& source);

    // Parse the entire program; throws ParseException on syntax error.
    Program parse();

    // Errors collected during recovery (synchronised parsing).
    const std::vector<ParseError>& errors() const { return errors_; }
    bool had_error() const { return !errors_.empty(); }

private:
    std::vector<Token>   tokens_;
    const std::string&   source_;
    int                  current_ = 0;
    std::vector<ParseError> errors_;

    // ── Token helpers ─────────────────────────────────────────────────
    Token& peek();
    Token& previous();
    bool   is_at_end();
    Token  advance();
    bool   check(TokenType t);
    bool   match(std::initializer_list<TokenType> types);
    Token  consume(TokenType t, const std::string& msg);
    ParseException error(const Token& tok, const std::string& msg);
    void   synchronise();
    std::string source_line(int line) const;

    // ── Grammar rules ─────────────────────────────────────────────────
    StmtPtr declaration();
    StmtPtr fn_declaration();
    StmtPtr let_declaration();
    StmtPtr statement();
    StmtPtr print_statement();
    StmtPtr return_statement();
    StmtPtr if_statement();
    StmtPtr while_statement();
    StmtPtr for_statement();
    StmtPtr expr_statement();
    StmtPtr block(int ln);

    ExprPtr expression();
    ExprPtr assignment();
    ExprPtr logic_or();
    ExprPtr logic_and();
    ExprPtr equality();
    ExprPtr comparison();
    ExprPtr term();
    ExprPtr factor();
    ExprPtr unary();
    ExprPtr call();
    ExprPtr primary();
};
