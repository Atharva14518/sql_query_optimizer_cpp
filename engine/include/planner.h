#pragma once
#include <memory>
#include <string>
#include <vector>
#include "ast.h"
#include "stats.h"

namespace sqlopt {

struct PlanNode{
    virtual ~PlanNode()=default;
    double est_rows=0.0;
    double cost=0.0;
    virtual std::string explain(int indent=0) const = 0;
};

struct ScanNode : PlanNode{
    std::string table;
    std::string alias;
    std::vector<std::string> filters;
    std::string explain(int indent=0) const override;
};

struct IndexScanNode : ScanNode{
    std::vector<std::string> index_cols;
    std::string explain(int indent=0) const override;
};

struct JoinNode : PlanNode{
    std::unique_ptr<PlanNode> left, right;
    std::string join_type="INNER";
    std::vector<std::string> conds;
    std::string algo="HASH";
    std::string explain(int indent=0) const override;
};

struct ProjectNode : PlanNode{
    std::unique_ptr<PlanNode> child;
    std::vector<std::string> items;
    std::string explain(int indent=0) const override;
};

struct FilterNode : PlanNode{
    std::unique_ptr<PlanNode> child;
    std::vector<std::string> conds;
    std::string explain(int indent=0) const override;
};

struct SortNode : PlanNode{
    std::unique_ptr<PlanNode> child;
    std::vector<OrderItem> order;
    std::string explain(int indent=0) const override;
};

struct LimitNode : PlanNode{
    std::unique_ptr<PlanNode> child;
    int limit=-1;
    std::string explain(int indent=0) const override;
};

struct DistinctNode : PlanNode{
    std::unique_ptr<PlanNode> child;
    std::string explain(int indent=0) const override;
};

struct HavingNode : PlanNode{
    std::unique_ptr<PlanNode> child;
    std::vector<std::string> conds;
    std::string explain(int indent=0) const override;
};

} // namespace sqlopt
