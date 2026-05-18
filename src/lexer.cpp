#include "lexer.h"
#include <stdexcept>
#include <unordered_map>

static const std::unordered_map<std::string, TokenType> KEYWORDS = {
    {"and",    TokenType::AND},
    {"or",     TokenType::OR},
    {"not",    TokenType::NOT},
    {"true",   TokenType::TRUE_KW},
    {"false",  TokenType::FALSE_KW},
    {"nil",    TokenType::NIL_KW},
    {"let",    TokenType::LET},
    {"fn",     TokenType::FN},
    {"return", TokenType::RETURN},
    {"if",     TokenType::IF},
    {"else",   TokenType::ELSE},
    {"while",  TokenType::WHILE},
    {"for",    TokenType::FOR},
    {"print",  TokenType::PRINT},
    {"input",  TokenType::INPUT},
};

const char* token_type_name(TokenType t) {
    switch (t) {
        case TokenType::LPAREN:     return "LPAREN";
        case TokenType::RPAREN:     return "RPAREN";
        case TokenType::LBRACE:     return "LBRACE";
        case TokenType::RBRACE:     return "RBRACE";
        case TokenType::COMMA:      return "COMMA";
        case TokenType::SEMICOLON:  return "SEMICOLON";
        case TokenType::COLON:      return "COLON";
        case TokenType::BANG:       return "BANG";
        case TokenType::BANG_EQ:    return "BANG_EQ";
        case TokenType::EQ:         return "EQ";
        case TokenType::EQ_EQ:      return "EQ_EQ";
        case TokenType::GT:         return "GT";
        case TokenType::GT_EQ:      return "GT_EQ";
        case TokenType::LT:         return "LT";
        case TokenType::LT_EQ:      return "LT_EQ";
        case TokenType::PLUS:       return "PLUS";
        case TokenType::MINUS:      return "MINUS";
        case TokenType::STAR:       return "STAR";
        case TokenType::SLASH:      return "SLASH";
        case TokenType::PERCENT:    return "PERCENT";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER:     return "NUMBER";
        case TokenType::STRING:     return "STRING";
        case TokenType::AND:        return "AND";
        case TokenType::OR:         return "OR";
        case TokenType::NOT:        return "NOT";
        case TokenType::TRUE_KW:    return "TRUE";
        case TokenType::FALSE_KW:   return "FALSE";
        case TokenType::NIL_KW:     return "NIL";
        case TokenType::LET:        return "LET";
        case TokenType::FN:         return "FN";
        case TokenType::RETURN:     return "RETURN";
        case TokenType::IF:         return "IF";
        case TokenType::ELSE:       return "ELSE";
        case TokenType::WHILE:      return "WHILE";
        case TokenType::FOR:        return "FOR";
        case TokenType::PRINT:      return "PRINT";
        case TokenType::INPUT:      return "INPUT";
        case TokenType::EOF_TOK:    return "EOF";
        case TokenType::ERROR:      return "ERROR";
        default:                    return "?";
    }
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

bool Lexer::is_at_end() const { return current_ >= (int)source_.size(); }
char Lexer::advance()         { return source_[current_++]; }
char Lexer::peek()      const { return is_at_end() ? '\0' : source_[current_]; }
char Lexer::peek_next() const {
    if (current_ + 1 >= (int)source_.size()) return '\0';
    return source_[current_ + 1];
}
bool Lexer::match(char expected) {
    if (is_at_end() || source_[current_] != expected) return false;
    current_++;
    return true;
}
std::string Lexer::current_lexeme() const {
    return source_.substr(start_, current_ - start_);
}

void Lexer::add_token(TokenType t) {
    tokens_.emplace_back(t, current_lexeme(), line_);
}
void Lexer::add_token(TokenType t, std::string lexeme) {
    tokens_.emplace_back(t, std::move(lexeme), line_);
}
void Lexer::error_token(const std::string& msg) {
    tokens_.emplace_back(TokenType::ERROR, msg, line_);
}

void Lexer::skip_whitespace_and_comments() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ': case '\r': case '\t': advance(); break;
            case '\n': line_++; advance(); break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    advance(); advance(); // consume /*
                    int depth = 1;
                    while (!is_at_end() && depth > 0) {
                        if (peek() == '/' && peek_next() == '*') { advance(); advance(); depth++; }
                        else if (peek() == '*' && peek_next() == '/') { advance(); advance(); depth--; }
                        else { if (peek() == '\n') line_++; advance(); }
                    }
                } else {
                    return;
                }
                break;
            default: return;
        }
    }
}

void Lexer::string_token() {
    std::string value;
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') line_++;
        if (peek() == '\\') {
            advance();
            switch (advance()) {
                case 'n':  value += '\n'; break;
                case 't':  value += '\t'; break;
                case 'r':  value += '\r'; break;
                case '"':  value += '"';  break;
                case '\\': value += '\\'; break;
                default:   value += '?';  break;
            }
        } else {
            value += advance();
        }
    }
    if (is_at_end()) { error_token("Unterminated string."); return; }
    advance(); // closing "
    tokens_.emplace_back(TokenType::STRING, value, line_);
}

void Lexer::number_token() {
    while (std::isdigit(peek())) advance();
    if (peek() == '.' && std::isdigit(peek_next())) {
        advance();
        while (std::isdigit(peek())) advance();
    }
    std::string lex = current_lexeme();
    Token tok(TokenType::NUMBER, lex, line_);
    tok.num_value = std::stod(lex);
    tokens_.push_back(std::move(tok));
}

void Lexer::identifier_token() {
    while (std::isalnum(peek()) || peek() == '_') advance();
    std::string lex = current_lexeme();
    auto it = KEYWORDS.find(lex);
    TokenType t = (it != KEYWORDS.end()) ? it->second : TokenType::IDENTIFIER;
    add_token(t, lex);
}

void Lexer::scan_token() {
    skip_whitespace_and_comments();
    if (is_at_end()) { add_token(TokenType::EOF_TOK, ""); return; }

    start_ = current_;
    char c = advance();

    switch (c) {
        case '(': add_token(TokenType::LPAREN);   break;
        case ')': add_token(TokenType::RPAREN);   break;
        case '{': add_token(TokenType::LBRACE);   break;
        case '}': add_token(TokenType::RBRACE);   break;
        case ',': add_token(TokenType::COMMA);    break;
        case ';': add_token(TokenType::SEMICOLON);break;
        case ':': add_token(TokenType::COLON);    break;
        case '+': add_token(TokenType::PLUS);     break;
        case '-': add_token(TokenType::MINUS);    break;
        case '*': add_token(TokenType::STAR);     break;
        case '/': add_token(TokenType::SLASH);    break;
        case '%': add_token(TokenType::PERCENT);  break;
        case '!': add_token(match('=') ? TokenType::BANG_EQ : TokenType::BANG); break;
        case '=': add_token(match('=') ? TokenType::EQ_EQ   : TokenType::EQ);   break;
        case '<': add_token(match('=') ? TokenType::LT_EQ   : TokenType::LT);   break;
        case '>': add_token(match('=') ? TokenType::GT_EQ   : TokenType::GT);   break;
        case '"': string_token(); break;
        default:
            if (std::isdigit(c)) { number_token(); }
            else if (std::isalpha(c) || c == '_') { identifier_token(); }
            else { error_token(std::string("Unexpected character '") + c + "'."); }
    }
}

std::vector<Token> Lexer::tokenise() {
    while (true) {
        scan_token();
        if (!tokens_.empty() && tokens_.back().type == TokenType::EOF_TOK) break;
        if (!tokens_.empty() && tokens_.back().type == TokenType::ERROR) break;
    }
    return std::move(tokens_);
}
