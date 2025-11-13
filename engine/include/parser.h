#pragma once
#include <string>
#include <vector>
#include "ast.h"
#include "lexer.h"

namespace sqlopt {

struct ParseError{ std::string message; int pos=-1; };

class Parser{
    std::vector<Token> toks; int i=0, n=0;
public:
    explicit Parser(std::vector<Token> t): toks(std::move(t)){ n=(int)toks.size(); }
    bool parse_query(Query &out, ParseError &err);
private:
    bool parse_select(SelectQuery &out, ParseError &err);
    bool parse_insert(InsertQuery &out, ParseError &err);
    bool parse_update(UpdateQuery &out, ParseError &err);
    bool parse_delete(DeleteQuery &out, ParseError &err);
};

} // namespace sqlopt
