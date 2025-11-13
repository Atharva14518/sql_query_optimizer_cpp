#include "parser.h"
#include "utils.h"
#include <unordered_set>

using namespace sqlopt;

static std::string lower(const std::string &s){ return to_lower(s); }
static bool is_kw(const Token &t, const char* kw){ return t.type==TokenType::KW && lower(t.text)==kw; }

bool Parser::parse_query(Query &out, ParseError &err){
    if (i >= n) { err = {"Empty query", -1}; return false; }
    if (is_kw(toks[i], "select")) {
        SelectQuery q;
        if (!parse_select(q, err)) return false;
        out = q;
    } else if (is_kw(toks[i], "insert")) {
        InsertQuery q;
        if (!parse_insert(q, err)) return false;
        out = q;
    } else if (is_kw(toks[i], "update")) {
        UpdateQuery q;
        if (!parse_update(q, err)) return false;
        out = q;
    } else if (is_kw(toks[i], "delete")) {
        DeleteQuery q;
        if (!parse_delete(q, err)) return false;
        out = q;
    } else {
        err = {"Expected SELECT, INSERT, UPDATE, or DELETE", toks[i].pos};
        return false;
    }
    return true;
}

bool Parser::parse_select(SelectQuery &out, ParseError &err){
    auto expect=[&](auto pred, const char* what)->bool{
        if(i<n && pred(toks[i])) return true;
        err = { std::string("Expected ")+what, i<n? toks[i].pos : -1};
        return false;
    };
    auto accept=[&](auto pred)->bool{ if(i<n && pred(toks[i])){ ++i; return true; } return false; };

    if(!expect([&](const Token&t){ return is_kw(t, "select"); }, "SELECT")) return false; ++i;

    if(i<n && is_kw(toks[i],"distinct")){ out.distinct=true; ++i; }

    while(i<n && !is_kw(toks[i],"from") && toks[i].type != TokenType::COMMA){
        SelectItem item;
        std::string expr;
    while(i<n && !is_kw(toks[i],"from") && toks[i].type != TokenType::COMMA && !is_kw(toks[i],"as") ){
            expr += toks[i].text;
            // Add space between tokens except for dots, parentheses, and stars
            if(i+1<n && !is_kw(toks[i+1],"from") && toks[i+1].type != TokenType::COMMA && !is_kw(toks[i+1],"as") && 
               toks[i+1].type != TokenType::DOT && toks[i].type != TokenType::DOT &&
               toks[i+1].type != TokenType::LPAREN && toks[i].type != TokenType::RPAREN &&
               toks[i+1].type != TokenType::RPAREN && toks[i].type != TokenType::LPAREN &&
               toks[i+1].type != TokenType::STAR && toks[i].type != TokenType::STAR) {
                expr += " ";
            }
            ++i;
        }
        item.expr = trim(expr);
        if(is_kw(toks[i],"as")){ ++i; if(i<n && toks[i].type==TokenType::IDENT){ item.alias = toks[i].text; ++i; } }
        else if(i<n && toks[i].type==TokenType::IDENT && !is_kw(toks[i],"from") && toks[i].type != TokenType::COMMA){ item.alias = toks[i].text; ++i; }
        out.select_items.push_back(item);
        if(accept([&](const Token&t){return t.type==TokenType::COMMA;})) continue; else break;
    }

    if(!expect([&](const Token&t){return is_kw(t,"from");}, "FROM")) return false; ++i;

    auto parse_table=[&]()->TableRef{
        TableRef tr;
        if(i>=n || toks[i].type!=TokenType::IDENT){ err={"Expected table name", i<n?toks[i].pos:-1}; return {}; }
        tr.name=lower(toks[i].text); ++i;
        if(accept([&](const Token&t){return is_kw(t,"as");})){
            if(i<n && toks[i].type==TokenType::IDENT){ tr.alias=lower(toks[i].text); ++i; } else { err={"Expected alias after AS", i<n?toks[i].pos:-1}; return {}; }
        } else if(i<n && toks[i].type==TokenType::IDENT) { tr.alias=lower(toks[i].text); ++i; }
        return tr;
    };

    out.from_table = parse_table();
    if(out.from_table.name.empty()) return false;
    
    // Handle comma-separated tables (convert to joins)
    while(i<n && toks[i].type==TokenType::COMMA) {
        ++i; // skip comma
        auto additional_table = parse_table();
        if(additional_table.name.empty()) return false;
        
        // Convert comma join to INNER JOIN
        JoinClause jc;
        jc.type = JoinType::INNER;
        jc.table = additional_table;
        // We'll need to infer join conditions from WHERE clause later
        jc.on_conds.push_back("1=1"); // placeholder - will be resolved in WHERE
        out.joins.push_back(jc);
    }

    while(i<n && (is_kw(toks[i],"join") || is_kw(toks[i],"inner") || is_kw(toks[i],"left") || is_kw(toks[i],"right") || is_kw(toks[i],"full") || is_kw(toks[i],"natural"))){
        JoinType jt = JoinType::INNER;
        if(is_kw(toks[i],"left")){ jt=JoinType::LEFT; ++i; if(is_kw(toks[i],"anti")){ jt=JoinType::LEFT_ANTI; ++i; } }
        else if(is_kw(toks[i],"right")){ jt=JoinType::RIGHT; ++i; if(is_kw(toks[i],"anti")){ jt=JoinType::RIGHT_ANTI; ++i; } }
        else if(is_kw(toks[i],"full")){ jt=JoinType::FULL; ++i; if(is_kw(toks[i],"outer")){ ++i; if(is_kw(toks[i],"anti")){ jt=JoinType::FULL_OUTER_ANTI; ++i; } } }
        else if(is_kw(toks[i],"natural")){ jt=JoinType::NATURAL; ++i; }
        else if(is_kw(toks[i],"inner")){ jt=JoinType::INNER; ++i; }
        if(!is_kw(toks[i],"join")){ err={"Expected JOIN", i<n?toks[i].pos:-1}; return false; }
        ++i;
        JoinClause jc; jc.type=jt;
        jc.table = parse_table();
        if(jc.table.name.empty()) return false;
        if(jt != JoinType::NATURAL){
            if(!expect([&](const Token&t){return is_kw(t,"on");}, "ON")) return false; ++i;
            std::string lhs, rhs, op;
            if(i<n && toks[i].type==TokenType::IDENT){ lhs=toks[i++].text; if(i<n && toks[i].type==TokenType::DOT){ ++i; if(i<n && toks[i].type==TokenType::IDENT){ lhs += "."+toks[i++].text; }}}
            if(i<n && toks[i].type==TokenType::OP){ op=toks[i++].text; }
            if(i<n && toks[i].type==TokenType::IDENT){ rhs=toks[i++].text; if(i<n && toks[i].type==TokenType::DOT){ ++i; if(i<n && toks[i].type==TokenType::IDENT){ rhs += "."+toks[i++].text; }}}
            if(lhs.empty()||rhs.empty()||op.empty()){ err={"Malformed JOIN ON condition", i<n?toks[i].pos:-1}; return false; }
            jc.on_conds.push_back(lhs+" "+op+" "+rhs);
        }
        out.joins.push_back(jc);
    }

    if(i<n && is_kw(toks[i],"where")){
        ++i;
        std::string accum;
        while(i<n && !(toks[i].type==TokenType::KW && (lower(toks[i].text)=="group"||lower(toks[i].text)=="order"||lower(toks[i].text)=="limit"))){
            if(toks[i].type==TokenType::KW && lower(toks[i].text)=="and"){ if(!accum.empty()){ out.where_conditions.push_back(accum); accum.clear(); } ++i; continue; }
            if(!accum.empty()) accum.push_back(' ');
            accum += toks[i].type==TokenType::STRING ? ("'"+toks[i].text+"'") : toks[i].text;
            ++i;
        }
        if(!accum.empty()) out.where_conditions.push_back(accum);
    }

    if(i<n && is_kw(toks[i],"group")){
        ++i; if(!expect([&](const Token&t){return is_kw(t,"by");}, "BY")) return false; ++i;
        while(i<n && toks[i].type==TokenType::IDENT){ 
            std::string col = toks[i++].text;
            // Handle dotted identifiers like table.column
            if(i<n && toks[i].type==TokenType::DOT){ 
                col += ".";
                ++i;
                if(i<n && toks[i].type==TokenType::IDENT){ 
                    col += toks[i++].text; 
                }
            }
            out.group_by.push_back(col); 
            if(i<n && toks[i].type==TokenType::COMMA){ ++i; } else break;
        }
    }

    if(i<n && is_kw(toks[i],"having")){
        ++i;
        std::string accum;
        while(i<n && !(toks[i].type==TokenType::KW && (lower(toks[i].text)=="order"||lower(toks[i].text)=="limit"))){
            if(toks[i].type==TokenType::COMMA){ ++i; continue; }
            if(toks[i].type==TokenType::KW && lower(toks[i].text)=="and"){ if(!accum.empty()){ out.having_conditions.push_back(accum); accum.clear(); } ++i; continue; }
            if(!accum.empty()) accum.push_back(' ');
            accum += toks[i].type==TokenType::STRING ? ("'"+toks[i].text+"'") : toks[i].text;
            ++i;
        }
        if(!accum.empty()) out.having_conditions.push_back(accum);
    }

    if(i<n && is_kw(toks[i],"order")){
        ++i; if(!expect([&](const Token&t){return is_kw(t,"by");}, "BY")) return false; ++i;
        while(i<n && toks[i].type==TokenType::IDENT){ OrderItem oi{toks[i++].text,true};
            if(i<n && toks[i].type==TokenType::KW && (lower(toks[i].text)=="asc"||lower(toks[i].text)=="desc")){ oi.asc = lower(toks[i].text)=="asc"; ++i; }
            out.order_by.push_back(oi); if(i<n && toks[i].type==TokenType::COMMA){ ++i; } else break;
        }
    }

    if(i<n && is_kw(toks[i],"limit")){
        ++i; if(i<n && toks[i].type==TokenType::NUMBER){ out.limit = std::stoi(toks[i++].text); }
        else { err={"Expected numeric LIMIT", i<n?toks[i].pos:-1}; return false; }
    }
    // Skip semicolon and any trailing whitespace/end tokens
    while(i < n && (toks[i].type == TokenType::SEMICOLON || toks[i].type == TokenType::END || toks[i].text.empty())) {
        ++i;
    }
    // Accept end of input - no error for missing semicolon
    if(i >= n) {
        return true;
    }
    // Only error if there are actual meaningful tokens left
    if(i < n && !toks[i].text.empty() && toks[i].type != TokenType::END){
        err = {"Extra tokens after query", toks[i].pos};
        return false;
    }
    return true;
}

bool Parser::parse_insert(InsertQuery &out, ParseError &err){
    auto expect=[&](auto pred, const char* what)->bool{
        if(i<n && pred(toks[i])) return true;
        err = { std::string("Expected ")+what, i<n? toks[i].pos : -1};
        return false;
    };
    auto accept=[&](auto pred)->bool{ if(i<n && pred(toks[i])){ ++i; return true; } return false; };

    if(!expect([&](const Token&t){ return is_kw(t, "insert"); }, "INSERT")) return false; ++i;
    if(!expect([&](const Token&t){ return is_kw(t, "into"); }, "INTO")) return false; ++i;
    if(i>=n || toks[i].type!=TokenType::IDENT){ err={"Expected table name", i<n?toks[i].pos:-1}; return false; }
    out.table = lower(toks[i++].text);
    if(accept([&](const Token&t){return t.type==TokenType::LPAREN;})){
        while(i<n && toks[i].type!=TokenType::RPAREN){
            if(toks[i].type==TokenType::IDENT) out.columns.push_back(toks[i++].text);
            if(!accept([&](const Token&t){return t.type==TokenType::COMMA;})) break;
        }
        if(!expect([&](const Token&t){return t.type==TokenType::RPAREN;}, ")")) return false; ++i;
    }
    if(!expect([&](const Token&t){ return is_kw(t, "values"); }, "VALUES")) return false; ++i;
    while(i<n && toks[i].type==TokenType::LPAREN){
        ++i;
        std::vector<std::string> row;
        while(i<n && toks[i].type!=TokenType::RPAREN){
            if(toks[i].type==TokenType::STRING || toks[i].type==TokenType::NUMBER || toks[i].type==TokenType::IDENT) row.push_back(toks[i++].text);
            if(!accept([&](const Token&t){return t.type==TokenType::COMMA;})) break;
        }
        if(!expect([&](const Token&t){return t.type==TokenType::RPAREN;}, ")")) return false; ++i;
        out.values.push_back(row);
        if(!accept([&](const Token&t){return t.type==TokenType::COMMA;})) break;
    }
    if(i < n && toks[i].type == TokenType::SEMICOLON) ++i;
    if(i < n && toks[i].type != TokenType::END){
        err = {"Extra tokens after query", toks[i].pos};
        return false;
    }
    return true;
}

bool Parser::parse_update(UpdateQuery &out, ParseError &err){
    auto expect=[&](auto pred, const char* what)->bool{
        if(i<n && pred(toks[i])) return true;
        err = { std::string("Expected ")+what, i<n? toks[i].pos : -1};
        return false;
    };
    auto accept=[&](auto pred)->bool{ if(i<n && pred(toks[i])){ ++i; return true; } return false; };

    if(!expect([&](const Token&t){ return is_kw(t, "update"); }, "UPDATE")) return false; ++i;
    if(i>=n || toks[i].type!=TokenType::IDENT){ err={"Expected table name", i<n?toks[i].pos:-1}; return false; }
    out.table = lower(toks[i++].text);
    if(!expect([&](const Token&t){ return is_kw(t, "set"); }, "SET")) return false; ++i;
    while(i<n && !is_kw(toks[i],"where") && toks[i].type != TokenType::SEMICOLON){
        std::string col, val;
        if(toks[i].type==TokenType::IDENT) col = toks[i++].text;
        if(!expect([&](const Token&t){return t.type==TokenType::OP && t.text=="=";}, "=")) return false; ++i;
        if(toks[i].type==TokenType::STRING || toks[i].type==TokenType::NUMBER || toks[i].type==TokenType::IDENT) val = toks[i++].text;
        out.set_clauses.emplace_back(col, val);
        if(!accept([&](const Token&t){return t.type==TokenType::COMMA;})) break;
    }
    if(accept([&](const Token&t){return is_kw(t,"where");})){
        std::string accum;
        while(i<n && toks[i].type != TokenType::SEMICOLON){
            if(!accum.empty()) accum.push_back(' ');
            accum += toks[i].type==TokenType::STRING ? ("'"+toks[i].text+"'") : toks[i].text;
            ++i;
        }
        if(!accum.empty()) out.where_conditions.push_back(accum);
    }
    if(i < n && toks[i].type == TokenType::SEMICOLON) ++i;
    if(i < n && toks[i].type != TokenType::END){
        err = {"Extra tokens after query", toks[i].pos};
        return false;
    }
    return true;
}

bool Parser::parse_delete(DeleteQuery &out, ParseError &err){
    auto expect=[&](auto pred, const char* what)->bool{
        if(i<n && pred(toks[i])) return true;
        err = { std::string("Expected ")+what, i<n? toks[i].pos : -1};
        return false;
    };
    auto accept=[&](auto pred)->bool{ if(i<n && pred(toks[i])){ ++i; return true; } return false; };

    if(!expect([&](const Token&t){ return is_kw(t, "delete"); }, "DELETE")) return false; ++i;
    if(!expect([&](const Token&t){ return is_kw(t, "from"); }, "FROM")) return false; ++i;
    if(i>=n || toks[i].type!=TokenType::IDENT){ err={"Expected table name", i<n?toks[i].pos:-1}; return false; }
    out.table = lower(toks[i++].text);
    if(accept([&](const Token&t){return is_kw(t,"where");})){
        std::string accum;
        while(i<n && toks[i].type != TokenType::SEMICOLON){
            if(!accum.empty()) accum.push_back(' ');
            accum += toks[i].type==TokenType::STRING ? ("'"+toks[i].text+"'") : toks[i].text;
            ++i;
        }
        if(!accum.empty()) out.where_conditions.push_back(accum);
    }
    if(i < n && toks[i].type == TokenType::SEMICOLON) ++i;
    if(i < n && toks[i].type != TokenType::END){
        err = {"Extra tokens after query", toks[i].pos};
        return false;
    }
    return true;
}
