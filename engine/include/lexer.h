#pragma once
#include <string>
#include <vector>

namespace sqlopt {

enum class TokenType { IDENT, NUMBER, STRING, STAR, COMMA, DOT, LPAREN, RPAREN, SEMICOLON, OP, KW, END };

struct Token{ TokenType type; std::string text; int pos; };

class Lexer{
    std::string s; int i=0; int n=0;
public:
    explicit Lexer(std::string input): s(std::move(input)), n((int)s.size()) {}
    std::vector<Token> tokenize();
};

} // namespace sqlopt
