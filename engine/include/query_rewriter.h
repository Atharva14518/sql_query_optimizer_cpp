#pragma once
#include "ast.h"
#include <string>
#include <vector>

namespace sqlopt {

class QueryRewriter {
public:
    QueryRewriter() = default;

    // Apply logical optimizations to the query
    void rewrite(SelectQuery& query);

private:
    // Convert comma joins to explicit JOIN syntax
    void convertCommaJoins(SelectQuery& query);
    
    // Reconstruct comma joins from WHERE conditions (for complex queries)
    void reconstructCommaJoins(SelectQuery& query);
    
    // Convert subqueries to JOINs for better performance
    void convertSubqueriesToJoins(SelectQuery& query);
    
    // Predicate pushdown: Move WHERE conditions closer to data sources
    void pushdownPredicates(SelectQuery& query);
    
    // Projection pushdown: Only select needed columns
    void pushdownProjections(SelectQuery& query);

    // Subquery flattening: Convert correlated subqueries to joins
    void flattenSubqueries(SelectQuery& query);

    // Constant folding: Pre-compute constant expressions
    void foldConstants(SelectQuery& query);

    // Join reordering: Optimize join sequence using heuristics
    void reorderJoins(SelectQuery& query);

    // Helper functions
    bool isPushablePredicate(const std::string& pred, const std::string& table_alias);
    std::vector<std::string> splitPredicates(const std::string& predicates);
    std::string joinPredicates(const std::vector<std::string>& preds, const std::string& op = " AND ");
};

} // namespace sqlopt
