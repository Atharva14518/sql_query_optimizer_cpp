#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <variant>

namespace sqlopt {

enum class JoinType { INNER, LEFT, RIGHT, FULL, NATURAL, LEFT_ANTI, RIGHT_ANTI, FULL_OUTER_ANTI };

struct Subquery {
    struct SelectQuery* q; // pointer to avoid recursion
};

struct TableRef{ std::string name; std::string alias; std::vector<std::string> pushedFilters; };

struct JoinClause {
    JoinType type;
    TableRef table;
    std::vector<std::string> on_conds;
};

struct OrderItem{ std::string expr; bool asc=true; };

struct SelectItem {
    std::string expr;
    std::string alias; // empty if no alias
};

struct SelectQuery{
    bool distinct = false;
    std::vector<SelectItem> select_items;
    TableRef from_table;
    std::vector<JoinClause> joins;
    std::vector<std::string> where_conditions;
    std::vector<std::string> group_by;
    std::vector<std::string> having_conditions;
    std::vector<OrderItem> order_by;
    int limit=-1;
    std::vector<Subquery> subqueries;
};

struct InsertQuery {
    std::string table;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> values; // list of rows
};

struct UpdateQuery {
    std::string table;
    std::vector<std::pair<std::string, std::string>> set_clauses; // col = expr
    std::vector<std::string> where_conditions;
};

struct DeleteQuery {
    std::string table;
    std::vector<std::string> where_conditions;
};

using Query = std::variant<SelectQuery, InsertQuery, UpdateQuery, DeleteQuery>;

} // namespace sqlopt
