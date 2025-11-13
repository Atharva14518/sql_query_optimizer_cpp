// sqlopt.cpp
// Compact SQL SELECT optimizer (rule-based + cost-based join ordering)
// Targets a wide, practical subset of SELECT queries (SELECT ... FROM ... [JOIN ... ON ...] [WHERE ...] [GROUP BY ...] [ORDER BY ...] [LIMIT ...])
// Author: assistant (converted & extended for Ayush's request)

#include "allheaders.h"
using namespace std;

// ----------------------------- Tokenizer -----------------------------
enum class Tok {
    END, IDENT, NUMBER, STRING,
    STAR, COMMA, DOT, LPAREN, RPAREN,
    EQ, NE, LT, LE, GT, GE,
    PLUS, MINUS, SLASH,
    SEMICOLON,
    KW_SELECT, KW_FROM, KW_WHERE, KW_JOIN, KW_ON, KW_INNER, KW_LEFT, KW_RIGHT, KW_FULL,
    KW_AND, KW_OR, KW_NOT, KW_AS,
    KW_DISTINCT, KW_GROUP, KW_BY, KW_ORDER, KW_LIMIT, KW_HAVING,
    KW_COUNT, KW_SUM, KW_LIKE,
    UNKNOWN
};

struct Token {
    Tok type;
    string text;
    int line, col;
};

string upper(const string &s) {
    string z = s;
    for (auto &c : z) c = toupper((unsigned char)c);
    return z;
}

struct Lexer {
    string in;
    size_t pos = 0;
    int line = 1, col = 1;
    Lexer() = default;
    Lexer(const string &s): in(s), pos(0), line(1), col(1) {}
    char peek() const { return pos < in.size() ? in[pos] : '\0'; }
    char get() {
        if (pos >= in.size()) return '\0';
        char c = in[pos++];
        if (c == '\n') { line++; col = 1; } else col++;
        return c;
    }
    void skip_ws() {
        while (isspace((unsigned char)peek())) get();
    }
    Token next_token() {
        skip_ws();
        Token tok{Tok::END, "", line, col};
        char c = peek();
        if (c == '\0') { tok.type = Tok::END; return tok; }
        if (isalpha((unsigned char)c) || c == '_' ) {
            int startcol = col;
            string id;
            while (isalnum((unsigned char)peek()) || peek() == '_' ) id.push_back(get());
            string up = upper(id);
            tok.text = id;
            tok.line = line; tok.col = startcol;
            if (up == "SELECT") tok.type = Tok::KW_SELECT;
            else if (up == "FROM") tok.type = Tok::KW_FROM;
            else if (up == "WHERE") tok.type = Tok::KW_WHERE;
            else if (up == "JOIN") tok.type = Tok::KW_JOIN;
            else if (up == "ON") tok.type = Tok::KW_ON;
            else if (up == "INNER") tok.type = Tok::KW_INNER;
            else if (up == "LEFT") tok.type = Tok::KW_LEFT;
            else if (up == "RIGHT") tok.type = Tok::KW_RIGHT;
            else if (up == "FULL") tok.type = Tok::KW_FULL;
            else if (up == "AND") tok.type = Tok::KW_AND;
            else if (up == "OR") tok.type = Tok::KW_OR;
            else if (up == "NOT") tok.type = Tok::KW_NOT;
            else if (up == "AS") tok.type = Tok::KW_AS;
            else if (up == "DISTINCT") tok.type = Tok::KW_DISTINCT;
            else if (up == "GROUP") tok.type = Tok::KW_GROUP;
            else if (up == "BY") tok.type = Tok::KW_BY;
            else if (up == "ORDER") tok.type = Tok::KW_ORDER;
            else if (up == "LIMIT") tok.type = Tok::KW_LIMIT;
            else if (up == "HAVING") tok.type = Tok::KW_HAVING;
            else if (up == "COUNT") tok.type = Tok::KW_COUNT;
            else if (up == "SUM") tok.type = Tok::KW_SUM;
            else if (up == "LIKE") tok.type = Tok::KW_LIKE;
            else tok.type = Tok::IDENT;
            return tok;
        }
        if (isdigit((unsigned char)c)) {
            int startcol = col;
            string num;
            while (isdigit((unsigned char)peek()) || peek() == '.') num.push_back(get());
            tok.type = Tok::NUMBER;
            tok.text = num;
            tok.line = line; tok.col = startcol;
            return tok;
        }
        if (c == '\'' || c == '"') {
            int startcol = col;
            char q = get();
            string s;
            while (peek() && peek() != q) {
                if (peek() == '\\') { get(); if (peek()) s.push_back(get()); }
                else s.push_back(get());
            }
            if (peek() == q) get(); // consume ending quote
            tok.type = Tok::STRING;
            tok.text = s;
            tok.line = line; tok.col = startcol;
            return tok;
        }
        // punctuation / operators
        int startcol = col;
        switch (get()) {
            case '*': tok.type = Tok::STAR; tok.text = "*"; break;
            case ',': tok.type = Tok::COMMA; tok.text = ","; break;
            case '.': tok.type = Tok::DOT; tok.text = "."; break;
            case '(' : tok.type = Tok::LPAREN; tok.text = "("; break;
            case ')' : tok.type = Tok::RPAREN; tok.text = ")"; break;
            case ';' : tok.type = Tok::SEMICOLON; tok.text = ";"; break;
            case '+': tok.type = Tok::PLUS; tok.text = "+"; break;
            case '-': tok.type = Tok::MINUS; tok.text = "-"; break;
            case '/': tok.type = Tok::SLASH; tok.text = "/"; break;
            case '=': tok.type = Tok::EQ; tok.text = "="; break;
            case '<':
                if (peek() == '=') { get(); tok.type = Tok::LE; tok.text = "<="; }
                else if (peek() == '>') { get(); tok.type = Tok::NE; tok.text = "<>"; }
                else { tok.type = Tok::LT; tok.text = "<"; }
                break;
            case '>':
                if (peek() == '=') { get(); tok.type = Tok::GE; tok.text = ">="; }
                else { tok.type = Tok::GT; tok.text = ">"; }
                break;
            default:
                tok.type = Tok::UNKNOWN;
                tok.text = string(1, in[pos-1]);
        }
        tok.line = line; tok.col = startcol;
        return tok;
    }
};

// ----------------------------- Simple AST / Query representation -----------------------------

struct Condition {
    string text;                   // textual form, e.g. "a.id = b.user_id" (used for re-generation & simple printing)
    set<string> referencedTables;  // set of table names/aliases referenced (extracted heuristically)
};

struct TableRef {
    string name;   // table name
    string alias;  // may be empty; if empty, alias == name when used
    vector<Condition> pushedFilters; // filters assigned to this table by pushdown (NOTE: these are conjunctive)
};

struct SelectQuery {
    vector<string> select_items; // column expressions as text
    vector<TableRef> tables;     // list from FROM clause (in parsed order)
    // joins: we parse explicit JOINs as well into tables (join predicates go to conditions)
    vector<Condition> where_conditions; // conjunctive list (split by AND)
    vector<string> group_by;
    vector<string> order_by;
    bool distinct = false;
    int limit = -1;
};

// ----------------------------- Parser -----------------------------
struct Parser {
    Lexer lex;
    Token cur;
    bool bad = false;
    string err_msg;
    Parser(const string &s): lex(s) { cur = lex.next_token(); }

    void advance() { cur = lex.next_token(); }
    bool match(Tok t) { return cur.type == t; }
    bool accept(Tok t) { if (match(t)) { advance(); return true; } return false; }
    bool expect(Tok t, const string &what = "") {
        if (match(t)) { advance(); return true; }
        bad = true;
        string s = "Syntax error";
        if (!what.empty()) s += ": expected " + what;
        s += " at token '" + cur.text + "'";
        err_msg = s;
        return false;
    }

    // parse simple select list items separated by commas; we accept '*' or identifiers or function calls like COUNT(x)
    vector<string> parse_select_list() {
        vector<string> items;
        while (true) {
            if (match(Tok::STAR)) {
                items.push_back("*");
                advance();
            } else if (match(Tok::IDENT) || match(Tok::KW_COUNT) || match(Tok::KW_SUM)) {
                // parse maybe function or dotted identifier
                string first = cur.text;
                advance();
                if (match(Tok::LPAREN)) {
                    // function call parse until matching )
                    string fn = first + "(";
                    advance();
                    int depth = 1;
                    while (depth > 0 && cur.type != Tok::END) {
                        fn += cur.text;
                        if (cur.type == Tok::LPAREN) depth++;
                        if (cur.type == Tok::RPAREN) depth--;
                        if (depth > 0) { /* add separating characters carefully */ }
                        advance();
                        if (depth > 0 && cur.type != Tok::RPAREN) {
                            // continue; lexer strips whitespace, so concatenation is fine
                        }
                    }
                    fn += ")";
                    // check for AS alias
                    if (match(Tok::KW_AS)) {
                        fn += " AS ";
                        advance();
                        if (match(Tok::IDENT)) {
                            fn += cur.text;
                            advance();
                        } else { bad = true; err_msg = "Expected identifier after AS"; return items; }
                    }
                    items.push_back(fn);
                } else {
                    // maybe dotted column like t.col
                    string expr = first;
                    if (match(Tok::DOT)) {
                        expr.push_back('.');
                        advance();
                        if (match(Tok::IDENT)) {
                            expr += cur.text;
                            advance();
                        } else { bad = true; err_msg = "Expected identifier after ."; return items; }
                    }
                    // check for AS alias
                    if (match(Tok::KW_AS)) {
                        expr += " AS ";
                        advance();
                        if (match(Tok::IDENT)) {
                            expr += cur.text;
                            advance();
                        } else { bad = true; err_msg = "Expected identifier after AS"; return items; }
                    }
                    items.push_back(expr);
                }
            } else {
                bad = true; err_msg = "Invalid select item near '" + cur.text + "'"; return items;
            }
            if (match(Tok::COMMA)) { advance(); continue; } else break;
        }
        return items;
    }

    // parse one table reference: name [AS] [alias]
    TableRef parse_table_ref() {
        TableRef t;
        if (!match(Tok::IDENT)) { bad = true; err_msg = "Expected table name"; return t; }
        t.name = cur.text; advance();
        if (match(Tok::KW_AS)) { advance(); if (!match(Tok::IDENT)) { bad=true; err_msg="Expected alias after AS"; return t; } t.alias = cur.text; advance(); }
        else if (match(Tok::IDENT)) { // alias without AS
            t.alias = cur.text; advance();
        } else {
            t.alias = t.name; // default alias
        }
        return t;
    }

    // Parse a simple binary condition in WHERE or ON: left op right.
    // Only handles many useful ops (=, <, >, <=, >=, <>, !=, LIKE). Right/left may be identifier or string/number.
    Condition parse_simple_condition() {
        Condition c;
        // capture tokens until next AND/OR or comma or end or RPAREN (top-level)
        // But we assume form: IDENT(.IDENT)? OP (IDENT(.IDENT)? | STRING | NUMBER)
        string left;
        if (match(Tok::IDENT)) { left = cur.text; advance(); if (match(Tok::DOT)) { left.push_back('.'); advance(); if (match(Tok::IDENT)) { left += cur.text; advance(); } }}
        else {
            // fallback: collect tokens until AND/OR
            if (match(Tok::STRING) || match(Tok::NUMBER)) {
                left = cur.text; advance();
            } else {
                // fallback read token
                left = cur.text; advance();
            }
        }
        // op
        string op;
        if (match(Tok::EQ) || match(Tok::LT) || match(Tok::GT) || match(Tok::LE) || match(Tok::GE) || match(Tok::NE) || match(Tok::KW_LIKE)) {
            if (match(Tok::EQ)) op = "=";
            else if (match(Tok::LT)) op = "<";
            else if (match(Tok::GT)) op = ">";
            else if (match(Tok::LE)) op = "<=";
            else if (match(Tok::GE)) op = ">=";
            else if (match(Tok::NE)) op = "<>";
            else if (match(Tok::KW_LIKE)) op = "LIKE";
            advance();
        } else {
            bad = true; err_msg = "Expected comparison operator in condition near '" + cur.text + "'"; return c;
        }
        string right;
        if (match(Tok::IDENT)) { right = cur.text; advance(); if (match(Tok::DOT)) { right.push_back('.'); advance(); if (match(Tok::IDENT)) { right += cur.text; advance(); } } }
        else if (match(Tok::STRING) || match(Tok::NUMBER)) { right = cur.text; advance(); }
        else { bad = true; err_msg = "Expected identifier/string/number on right side of condition"; return c; }
        // produce textual condition
        c.text = left + " " + op + " " + right;
        // extract referenced table names heuristically from left & right if they contain '.'
        auto collect = [&](const string &s) {
            size_t p = s.find('.');
            if (p != string::npos) {
                string t = s.substr(0,p);
                // strip potential quotes
                c.referencedTables.insert(t);
            }
        };
        collect(left); collect(right);
        return c;
    }

    vector<Condition> parse_where_conditions() {
        vector<Condition> conds;
        if (!expect(Tok::KW_WHERE, "WHERE")) return conds;
        while (true) {
            Condition c = parse_simple_condition();
            if (bad) return conds;
            conds.push_back(c);
            // accept AND (we only decompose conjunctive filters for reliable pushdown)
            if (match(Tok::KW_AND)) { advance(); continue; }
            break;
        }
        return conds;
    }

    SelectQuery parse_select() {
        SelectQuery q;
        if (!expect(Tok::KW_SELECT, "SELECT")) return q;
        // optional DISTINCT
        if (match(Tok::KW_DISTINCT)) { q.distinct = true; advance(); }
        q.select_items = parse_select_list(); if (bad) return q;
        // FROM clause required
        if (!expect(Tok::KW_FROM, "FROM")) return q;
        // parse first table
        TableRef t0 = parse_table_ref(); if (bad) return q;
        q.tables.push_back(t0);

        // accept a chain of JOINS or comma-separated tables
        while (true) {
            if (match(Tok::COMMA)) {
                advance();
                TableRef t = parse_table_ref(); if (bad) return q;
                q.tables.push_back(t);
                continue;
            }
            if (match(Tok::KW_INNER) || match(Tok::KW_LEFT) || match(Tok::KW_RIGHT) || match(Tok::KW_FULL) || match(Tok::KW_JOIN)) {
                // skip optional INNER/LEFT/RIGHT/FULL
                bool hasPrefix = false;
                if (match(Tok::KW_INNER) || match(Tok::KW_LEFT) || match(Tok::KW_RIGHT) || match(Tok::KW_FULL)) { hasPrefix = true; advance(); }
                if (!match(Tok::KW_JOIN)) { if (hasPrefix && match(Tok::KW_JOIN)) { /* ok */ } else { /* maybe alias */ } }
                if (match(Tok::KW_JOIN)) { advance(); }
                // next table ref
                TableRef t = parse_table_ref(); if (bad) return q;
                q.tables.push_back(t);
                // expect ON condition optionally
                if (match(Tok::KW_ON)) { // we'll parse ON condition and add to where conditions (join predicates)
                    advance();
                    Condition cj = parse_simple_condition(); if (bad) return q;
                    q.where_conditions.push_back(cj); // collect join predicate among where_conditions (we'll classify later)
                }
                continue;
            }
            break;
        }

        // optional WHERE
        if (match(Tok::KW_WHERE)) {
            vector<Condition> wc = parse_where_conditions();
            if (bad) return q;
            // append conditions
            for (auto &c : wc) q.where_conditions.push_back(c);
        }

        // optional GROUP BY (simple)
        if (match(Tok::KW_GROUP)) {
            advance();
            if (!expect(Tok::KW_BY, "BY")) return q;
            // parse simple comma-separated identifiers
            while (true) {
                if (!match(Tok::IDENT)) { bad = true; err_msg = "GROUP BY: expected identifier"; return q; }
                q.group_by.push_back(cur.text); advance();
                if (match(Tok::COMMA)) { advance(); continue; } else break;
            }
        }
        // optional ORDER BY
        if (match(Tok::KW_ORDER)) {
            advance();
            if (!expect(Tok::KW_BY, "BY")) return q;
            while (true) {
                if (!match(Tok::IDENT)) { bad=true; err_msg="ORDER BY: expected identifier"; return q; }
                q.order_by.push_back(cur.text); advance();
                if (match(Tok::COMMA)) { advance(); continue; } else break;
            }
        }
        // optional LIMIT
        if (match(Tok::KW_LIMIT)) {
            advance();
            if (!match(Tok::NUMBER)) { bad=true; err_msg="LIMIT: expected number"; return q; }
            q.limit = stoi(cur.text);
            advance();
        }
        return q;
    }
};

// ----------------------------- Table statistics (simple) -----------------------------
struct TableStats {
    string name;
    double row_count = 100000; // default
    map<string,double> distinct_vals; // per column distinct count if known
};

// For simplicity program uses default stats; user could extend to load stats from file
map<string, TableStats> default_stats;

// estimate selectivity for a condition heuristically
double estimate_condition_selectivity(const Condition &c, const TableStats &statsForOneTable = TableStats()) {
    // crude heuristics:
    // if equality condition and references one table with column distinct count known -> 1/distinct
    // otherwise equality -> 0.1, inequality -> 0.2, LIKE -> 0.1
    string txt = c.text;
    string op;
    if (txt.find(" = ") != string::npos) op = "=";
    else if (txt.find("<>") != string::npos || txt.find(" != ") != string::npos) op = "<>";
    else if (txt.find("<=") != string::npos) op = "<=";
    else if (txt.find(">=") != string::npos) op = ">=";
    else if (txt.find(" < ") != string::npos) op = "<";
    else if (txt.find(" > ") != string::npos) op = ">";
    else if (txt.find(" LIKE ") != string::npos) op = "LIKE";
    else op = "";

    if (c.referencedTables.size() == 1) {
        // try get distinct for referenced column if present in statsForOneTable (best effort)
        // parse column name after dot
        size_t p = txt.find('.');
        if (p != string::npos) {
            size_t sp = txt.find(' ', p);
            string col = txt.substr(p+1, (sp==string::npos? string::npos : sp-(p+1)));
            auto it = statsForOneTable.distinct_vals.find(col);
            if (it != statsForOneTable.distinct_vals.end() && it->second > 1.0) {
                if (op == "=") return 1.0 / it->second;
                if (op == "LIKE") return 0.1;
            }
        }
    }
    if (op == "=") return 0.05; // equality heuristic
    if (op == "<>" || op == "!=") return 0.9;
    if (op == "LIKE") return 0.1;
    // inequality
    return 0.2;
}

// ----------------------------- Logical Plan structures & utilities -----------------------------
struct Plan {
    enum Type { Scan, Filter, Join, Project } type;
    // for Scan
    string table; string alias;
    double rows = 1000;
    vector<Condition> local_filters; // applied to this scan
    // for Join
    shared_ptr<Plan> left, right;
    vector<Condition> join_conditions; // equality predicates connecting left/right
    // for Project
    vector<string> proj_items;

    // cost metric (lower is better)
    double cost = 0.0;

    string repr() const {
        // readable plan string (recursive)
        ostringstream os;
        if (type == Scan) {
            os << "Scan(" << table << " AS " << alias;
            if (!local_filters.empty()) {
                os << " FILTERS=[";
                for (size_t i=0;i<local_filters.size();++i) { os << local_filters[i].text << (i+1<local_filters.size() ? ", " : ""); }
                os << "]";
            }
            os << ") rows=" << (long)rows;
        } else if (type == Join) {
            os << "Join(rows=" << (long)rows << ", cost=" << cost << ", conds=[";
            for (size_t i=0;i<join_conditions.size();++i) os << join_conditions[i].text << (i+1<join_conditions.size()? ", ":"");
            os << "])\n  L-> " << left->repr() << "\n  R-> " << right->repr();
        } else if (type == Project) {
            os << "Project(items=[";
            for (size_t i=0;i<proj_items.size();++i) os << proj_items[i] << (i+1<proj_items.size()? ", ":"");
            os << "])\n  -> " << (left? left->repr() : string("<null>"));
        }
        return os.str();
    }
};

// helper: create scan node
shared_ptr<Plan> make_scan(const TableRef &t) {
    auto p = make_shared<Plan>();
    p->type = Plan::Scan;
    p->table = t.name;
    p->alias = t.alias.empty() ? t.name : t.alias;
    // assign rows from stats if present
    if (default_stats.count(t.name)) p->rows = default_stats[t.name].row_count;
    else p->rows = 100000; // conservative default
    p->local_filters = t.pushedFilters;
    // apply filter selectivity estimation
    // combine selectivities multiplicatively (conservative)
    double sel = 1.0;
    if (!p->local_filters.empty()) {
        TableStats ts;
        if (default_stats.count(t.name)) ts = default_stats[t.name];
        for (auto &c : p->local_filters) {
            double s = estimate_condition_selectivity(c, ts);
            sel *= s;
        }
    }
    p->rows = max(1.0, p->rows * sel);
    p->cost = p->rows; // simplistic
    return p;
}

// join two plans using join_conditions; estimate rows using heuristics
shared_ptr<Plan> make_join(shared_ptr<Plan> left, shared_ptr<Plan> right, const vector<Condition> &join_conds) {
    auto p = make_shared<Plan>();
    p->type = Plan::Join;
    p->left = left; p->right = right;
    p->join_conditions = join_conds;
    // estimate selectivity from join conds: for equality join, use 1/max(distincts) heuristic if stats available
    double sel = 1.0;
    // For simplicity: if we have at least one equality predicate, reduce by 1/100 or so depending on sizes
    if (!join_conds.empty()) {
        // if both scans and unique-ish, use low selectivity
        sel = 0.01; // default selectivity for join predicate
        // More sophisticated heuristics could inspect distinct counts per column
    } else {
        // cross join, very expensive
        sel = 1.0;
    }
    double est = max(1.0, left->rows * right->rows * sel);
    p->rows = est;
    // cost: sum of child costs + estimated output rows
    p->cost = left->cost + right->cost + p->rows;
    return p;
}

// ----------------------------- Optimizer: rule-based + cost-based join ordering -----------------------------

struct TransformLogEntry {
    string name;
    string desc;
    string before;
    string after;
};
vector<TransformLogEntry> transformLog;

void log_transform(const string &rule, const string &desc, const string &before, const string &after) {
    transformLog.push_back({rule, desc, before, after});
}

// classify conditions: build join predicates (between two different tables/aliases) and per-table local filters
void classify_conditions(SelectQuery &q, vector<pair<pair<int,int>, Condition>> &joinPreds) {
    // map alias->index
    unordered_map<string,int> aliasIndex;
    for (int i=0;i<(int)q.tables.size();++i) {
        string a = q.tables[i].alias.empty() ? q.tables[i].name : q.tables[i].alias;
        aliasIndex[a] = i;
    }

    vector<Condition> remaining;
    for (auto &c : q.where_conditions) {
        if ((int)c.referencedTables.size() == 2) {
            // determine which two
            vector<string> vt(c.referencedTables.begin(), c.referencedTables.end());
            string A = vt[0], B = vt[1];
            if (aliasIndex.count(A) && aliasIndex.count(B)) {
                int ai = aliasIndex[A], bi = aliasIndex[B];
                // store join predicate keyed by indices (ordered)
                joinPreds.push_back({{min(ai,bi), max(ai,bi)}, c});
                continue;
            }
        } else if ((int)c.referencedTables.size() == 1) {
            string A = *c.referencedTables.begin();
            if (aliasIndex.count(A)) {
                int ai = aliasIndex[A];
                q.tables[ai].pushedFilters.push_back(c);
                // note in transform log (we will log later)
                continue;
            }
        }
        // else ambiguous or global filter
        remaining.push_back(c);
    }
    // set q.where_conditions to remaining (global filters & join predicates removed)
    q.where_conditions = remaining;
}

// join-order DP (exact for up to n<=10 realistically)
struct DPEntry {
    bool ok = false;
    shared_ptr<Plan> plan;
    double cost = 1e308;
};

shared_ptr<Plan> cost_based_join_ordering(SelectQuery &q, vector<pair<pair<int,int>, Condition>> &joinPreds) {
    int n = (int)q.tables.size();
    if (n == 0) return nullptr;
    // pre-build adjacency map join conditions between pairs
    map<pair<int,int>, vector<Condition>> joinMap;
    for (auto &jp : joinPreds) {
        joinMap[jp.first].push_back(jp.second);
    }
    // base scans
    vector<shared_ptr<Plan>> base(n);
    for (int i=0;i<n;++i) base[i] = make_scan(q.tables[i]);

    int FULL = 1<<n;
    vector<DPEntry> dp(FULL);
    // initialize singletons
    for (int i=0;i<n;++i) {
        int m = 1<<i;
        dp[m].ok = true;
        dp[m].plan = base[i];
        dp[m].cost = base[i]->cost;
    }

    // iterate increasing sizes
    for (int mask=1; mask < FULL; ++mask) {
        if (__builtin_popcount((unsigned)mask) == 1) continue;
        // try partition mask into two non-empty submasks
        for (int left = (mask-1) & mask; left; left = (left-1) & mask) {
            int right = mask ^ left;
            if (!dp[left].ok || !dp[right].ok) continue;
            // check if there is a join predicate between any table in left and any in right
            vector<Condition> connectingConds;
            bool hasJoinPred = false;
            for (int i=0;i<n;++i) if (left & (1<<i)) {
                for (int j=0;j<n;++j) if (right & (1<<j)) {
                    pair<int,int> key = {min(i,j), max(i,j)};
                    if (joinMap.count(key)) {
                        hasJoinPred = true;
                        for (auto &c : joinMap[key]) connectingConds.push_back(c);
                    }
                }
            }
            // allow cross join too (no connectingConds) but penalize
            double penalty = hasJoinPred ? 1.0 : 1000.0;
            auto cand = make_join(dp[left].plan, dp[right].plan, connectingConds);
            cand->cost *= penalty;
            double candCost = dp[left].cost + dp[right].cost + cand->rows * (hasJoinPred ? 1.0 : 10.0);
            if (!dp[mask].ok || candCost < dp[mask].cost) {
                dp[mask].ok = true;
                dp[mask].plan = cand;
                dp[mask].cost = candCost;
            }
        }
    }
    if (dp[FULL-1].ok) return dp[FULL-1].plan;
    // fallback: left-to-right greedy join
    shared_ptr<Plan> acc = base[0];
    for (int i=1;i<n;++i) {
        vector<Condition> conds;
        pair<int,int> key = {min(0,i), max(0,i)};
        if (joinMap.count(key)) conds = joinMap[key];
        acc = make_join(acc, base[i], conds);
    }
    return acc;
}

// rule-based: merge adjacent projects (we have simple project model), constant folding (1=1)
void apply_rule_based_transforms(SelectQuery &q) {
    // example: constant folding - remove trivial "1 = 1" conditions
    vector<Condition> keep;
    for (auto &c : q.where_conditions) {
        string t = c.text;
        auto gone = [](const string &x){ string z; for (char ch: x) if (!isspace((unsigned char)ch)) z.push_back(toupper((unsigned char)ch)); return z; };
        string cleaned;
        for (char ch: t) if (!isspace((unsigned char)ch)) cleaned.push_back(toupper((unsigned char)ch));
        if (cleaned == "1=1") {
            log_transform("constant_fold", "Removed trivially true filter", c.text, "<removed>");
            continue;
        }
        keep.push_back(c);
    }
    q.where_conditions = keep;
}

// Generate optimized SQL from chosen join plan. We'll convert scans with pushed filters into inline views to show effect of pushdown.
string plan_to_sql(shared_ptr<Plan> plan) {
    if (!plan) return "";
    ostringstream os;
    if (plan->type == Plan::Scan) {
        // if we have local filters, produce inline view
        if (!plan->local_filters.empty()) {
            os << "(SELECT * FROM " << plan->table << " AS " << plan->alias << " WHERE ";
            for (size_t i=0;i<plan->local_filters.size();++i) {
                os << plan->local_filters[i].text;
                if (i+1 < plan->local_filters.size()) os << " AND ";
            }
            os << ") AS " << plan->alias;
        } else {
            os << plan->table << " AS " << plan->alias;
        }
    } else if (plan->type == Plan::Join) {
        // generate left and right SQL and ON clause from join_conditions
        string L = plan_to_sql(plan->left);
        string R = plan_to_sql(plan->right);
        os << "(" << L << " JOIN " << R;
        if (!plan->join_conditions.empty()) {
            os << " ON ";
            for (size_t i=0;i<plan->join_conditions.size();++i) {
                os << plan->join_conditions[i].text;
                if (i+1 < plan->join_conditions.size()) os << " AND ";
            }
        }
        os << ")";
    } else if (plan->type == Plan::Project) {
        // not used for top-level FROM generation
        os << plan_to_sql(plan->left);
    }
    return os.str();
}

// ----------------------------- Main CLI -----------------------------
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "SQL Optimizer (C++ prototype). Enter SELECT queries. Type exit or quit to stop.\n\n";

    // Example preloaded stats (user can extend code to load real stats)
    default_stats["users"] = TableStats{"users", 100000, {{"id", 100000}, {"age", 100}}};
    default_stats["orders"] = TableStats{"orders", 500000, {{"id", 500000}, {"user_id", 100000}, {"status", 10}}};
    default_stats["products"] = TableStats{"products", 20000, {{"id", 20000}}};
    default_stats["employee"] = TableStats{"employee", 10000, {{"emp_id", 10000}, {"emp_name", 5000}, {"manager_id", 1000}}};

    string line;
    while (true) {
        cout << "sql> ";
        if (!std::getline(cin, line)) break;
        if (line.size() == 0) continue;
        string tline = line;
        for (auto &c : tline) c = tolower((unsigned char)c);
        if (tline == "quit" || tline == "exit") break;

        Parser parser(line);
        SelectQuery q = parser.parse_select();
        if (parser.bad) {
            cout << "❌ Parse error: " << parser.err_msg << "\n";
            continue;
        }

        cout << "✅ Parsed successfully.\n";
        cout << "\n-- Parsed Query --\n";
        cout << "SELECT ";
        for (size_t i=0;i<q.select_items.size();++i) cout << q.select_items[i] << (i+1<q.select_items.size()? ", " : "");
        cout << "\nFROM ";
        for (size_t i=0;i<q.tables.size();++i) {
            cout << q.tables[i].name << " AS " << (q.tables[i].alias.empty()? q.tables[i].name : q.tables[i].alias);
            if (i+1<q.tables.size()) cout << ", ";
        }
        cout << "\nWHERE ";
        if (q.where_conditions.empty()) cout << "<none>";
        else { for (size_t i=0;i<q.where_conditions.size();++i) cout << q.where_conditions[i].text << (i+1<q.where_conditions.size()? " AND ": ""); }
        cout << "\n\n";

        // Apply rule-based transforms (constant-folding etc.)
        apply_rule_based_transforms(q);

        // classify conditions into join predicates and table filters; push them into table.pushedFilters
        vector<pair<pair<int,int>, Condition>> joinPreds;
        classify_conditions(q, joinPreds);

        // log pushdowns
        for (size_t i=0;i<q.tables.size();++i) {
            if (!q.tables[i].pushedFilters.empty()) {
                string before = "<original WHERE>";
                string after;
                for (size_t j=0;j<q.tables[i].pushedFilters.size();++j) {
                    if (j) after += " AND ";
                    after += q.tables[i].pushedFilters[j].text;
                }
                log_transform("selection_pushdown", "Pushed filters into table scan " + q.tables[i].alias, before, after);
            }
        }

        // Cost-based join ordering
        auto bestPlan = cost_based_join_ordering(q, joinPreds);

        cout << "--- Optimizer Trace ---\n";
        for (size_t i=0;i<transformLog.size();++i) {
            cout << i+1 << ". [" << transformLog[i].name << "] " << transformLog[i].desc << "\n";
            cout << "    before: " << transformLog[i].before << "\n";
            cout << "    after:  " << transformLog[i].after << "\n";
        }
        transformLog.clear();

        cout << "\n--- Chosen Plan ---\n";
        if (bestPlan) cout << bestPlan->repr() << "\n";
        else cout << "<no plan>\n";

        // Build optimized SQL
        string from_sql = plan_to_sql(bestPlan);
        ostringstream optimizedSQL;
        optimizedSQL << "SELECT ";
        for (size_t i=0;i<q.select_items.size();++i) optimizedSQL << q.select_items[i] << (i+1<q.select_items.size()? ", " : "");
        optimizedSQL << "\nFROM " << from_sql;
        // remaining global WHERE (those ambiguous or multi-table not pushed)
        if (!q.where_conditions.empty()) {
            optimizedSQL << "\nWHERE ";
            for (size_t i=0;i<q.where_conditions.size();++i) {
                optimizedSQL << q.where_conditions[i].text;
                if (i+1<q.where_conditions.size()) optimizedSQL << " AND ";
            }
        }
        if (!q.group_by.empty()) {
            optimizedSQL << "\nGROUP BY ";
            for (size_t i=0;i<q.group_by.size();++i) optimizedSQL << q.group_by[i] << (i+1<q.group_by.size()? ", ":"");
        }
        if (!q.order_by.empty()) {
            optimizedSQL << "\nORDER BY ";
            for (size_t i=0;i<q.order_by.size();++i) optimizedSQL << q.order_by[i] << (i+1<q.order_by.size()? ", ":"");
        }
        if (q.limit >= 0) optimizedSQL << "\nLIMIT " << q.limit;

        cout << "\n--- Optimized SQL ---\n";
        cout << optimizedSQL.str() << "\n\n";
    }

    cout << "Goodbye.\n";
    return 0;
}
