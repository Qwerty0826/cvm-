#pragma once
#include "common.h"

// ── Token types ───────────────────────────────────────────────────────────────
enum class TokenType {
    // Single-character
    LPAREN, RPAREN, LBRACE, RBRACE, COMMA, SEMICOLON, COLON,
    // One- or two-character operators
    BANG, BANG_EQ,
    EQ, EQ_EQ,
    GT, GT_EQ,
    LT, LT_EQ,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    // Literals
    IDENTIFIER, NUMBER, STRING,
    // Keywords
    AND, OR, NOT,
    TRUE_KW, FALSE_KW, NIL_KW,
    LET, FN, RETURN,
    IF, ELSE, WHILE, FOR,
    PRINT, INPUT,
    // End / error
    EOF_TOK, ERROR,
};

const char* token_type_name(TokenType t);

struct Token {
    TokenType   type;
    std::string lexeme;
    int         line;
    // For NUMBER tokens the value is pre-parsed.
    double      num_value = 0.0;

    Token(TokenType t, std::string lex, int ln)
        : type(t), lexeme(std::move(lex)), line(ln) {}
};

// ── Lexer ─────────────────────────────────────────────────────────────────────
class Lexer {
public:
    explicit Lexer(std::string source);

    // Tokenise the entire source and return all tokens.
    std::vector<Token> tokenise();

    // Source text (needed for error reporting with context).
    const std::string& source() const { return source_; }

private:
    std::string source_;
    int         start_   = 0;
    int         current_ = 0;
    int         line_    = 1;

    std::vector<Token> tokens_;

    bool   is_at_end()          const;
    char   advance();
    char   peek()               const;
    char   peek_next()          const;
    bool   match(char expected);
    void   skip_whitespace_and_comments();

    void   scan_token();
    void   string_token();
    void   number_token();
    void   identifier_token();

    void   add_token(TokenType t);
    void   add_token(TokenType t, std::string lexeme);
    void   error_token(const std::string& msg);

    std::string current_lexeme() const;
};
