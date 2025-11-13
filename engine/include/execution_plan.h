#pragma once
#include <memory>
#include <vector>
#include <string>
#include <iostream>

namespace sqlopt {

// Forward declarations
struct PlanNode;
struct ScanNode;
struct IndexScanNode;
struct JoinNode;
struct FilterNode;
struct ProjectNode;
struct SortNode;
struct AggregateNode;

// Plan node types
enum class PlanNodeType {
    SCAN,
    INDEX_SCAN,
    JOIN,
    FILTER,
    PROJECT,
    SORT,
    AGGREGATE,
    LIMIT
};

// Base plan node
struct PlanNode {
    PlanNodeType type;
    double estimated_cost = 0.0;
    size_t estimated_cardinality = 0;
    std::vector<std::string> output_columns;

    PlanNode(PlanNodeType t) : type(t) {}
    virtual ~PlanNode() = default;

    virtual void explain(int indent = 0) const = 0;
};

// Table scan node
struct ScanNode : PlanNode {
    std::string table;
    std::string alias;

    ScanNode(const std::string& t, const std::string& a = "")
        : PlanNode(PlanNodeType::SCAN), table(t), alias(a) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Scan(table=" << table;
        if (!alias.empty()) std::cout << " AS " << alias;
        std::cout << ", rows=" << estimated_cardinality << ", cost=" << estimated_cost << ")\n";
    }
};

// Index scan node
struct IndexScanNode : PlanNode {
    std::string table;
    std::string alias;
    std::string index_column;

    IndexScanNode(const std::string& t, const std::string& idx_col, const std::string& a = "")
        : PlanNode(PlanNodeType::INDEX_SCAN), table(t), alias(a), index_column(idx_col) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "IndexScan " << table << " using " << index_column;
        if (!alias.empty()) std::cout << " AS " << alias;
        std::cout << " (cost: " << estimated_cost << ", rows: " << estimated_cardinality << ")\n";
    }
};

// Join node
struct JoinNode : PlanNode {
    std::string join_type; // "inner", "left", "right", "full"
    std::unique_ptr<PlanNode> left;
    std::unique_ptr<PlanNode> right;
    std::vector<std::string> conditions;

    JoinNode(const std::string& jt, std::unique_ptr<PlanNode> l, std::unique_ptr<PlanNode> r,
             const std::vector<std::string>& conds)
        : PlanNode(PlanNodeType::JOIN), join_type(jt), left(std::move(l)), right(std::move(r)), conditions(conds) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << join_type << " Join(algo=" << join_type << ", rows=" << estimated_cardinality << ", cost=" << estimated_cost << ")\n";
        if (left) {
            try {
                left->explain(indent + 2);
            } catch (...) {
                std::cout << std::string(indent + 2, ' ') << "<error in left child>\n";
            }
        }
        if (right) {
            try {
                right->explain(indent + 2);
            } catch (...) {
                std::cout << std::string(indent + 2, ' ') << "<error in right child>\n";
            }
        }
    }
};

// Filter node
struct FilterNode : PlanNode {
    std::unique_ptr<PlanNode> child;
    std::vector<std::string> conditions;

    FilterNode(std::unique_ptr<PlanNode> c, const std::vector<std::string>& conds)
        : PlanNode(PlanNodeType::FILTER), child(std::move(c)), conditions(conds) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Filter";
        std::cout << " (cost: " << estimated_cost << ", rows: " << estimated_cardinality << ")\n";
        if (child) child->explain(indent + 2);
    }
};

// Project node
struct ProjectNode : PlanNode {
    std::unique_ptr<PlanNode> child;
    std::vector<std::string> projections;

    ProjectNode(std::unique_ptr<PlanNode> c, const std::vector<std::string>& projs)
        : PlanNode(PlanNodeType::PROJECT), child(std::move(c)), projections(projs) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Project(rows=" << estimated_cardinality << ", cost=" << estimated_cost << ", items=[";
        for (size_t i = 0; i < projections.size() && i < 3; ++i) {  // Limit to avoid long output
            std::cout << projections[i];
            if (i + 1 < projections.size() && i + 1 < 3) std::cout << ", ";
        }
        if (projections.size() > 3) std::cout << "...";
        std::cout << "])\n";
        if (child) {
            try {
                child->explain(indent + 2);
            } catch (...) {
                std::cout << std::string(indent + 2, ' ') << "<error in child node>\n";
            }
        }
    }
};

// Sort node
struct SortNode : PlanNode {
    std::unique_ptr<PlanNode> child;
    std::vector<std::string> sort_keys;
    std::vector<bool> ascending;

    SortNode(std::unique_ptr<PlanNode> c, const std::vector<std::string>& keys,
             const std::vector<bool>& asc)
        : PlanNode(PlanNodeType::SORT), child(std::move(c)), sort_keys(keys), ascending(asc) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Sort";
        std::cout << " (cost: " << estimated_cost << ", rows: " << estimated_cardinality << ")\n";
        if (child) child->explain(indent + 2);
    }
};

// Aggregate node
struct AggregateNode : PlanNode {
    std::unique_ptr<PlanNode> child;
    std::vector<std::string> group_by;
    std::vector<std::string> aggregates;

    AggregateNode(std::unique_ptr<PlanNode> c, const std::vector<std::string>& gb,
                  const std::vector<std::string>& aggs)
        : PlanNode(PlanNodeType::AGGREGATE), child(std::move(c)), group_by(gb), aggregates(aggs) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Aggregate";
        std::cout << " (cost: " << estimated_cost << ", rows: " << estimated_cardinality << ")\n";
        if (child) child->explain(indent + 2);
    }
};

// Limit node
struct LimitNode : PlanNode {
    std::unique_ptr<PlanNode> child;
    size_t limit_count;

    LimitNode(std::unique_ptr<PlanNode> c, size_t limit)
        : PlanNode(PlanNodeType::LIMIT), child(std::move(c)), limit_count(limit) {}

    void explain(int indent = 0) const override {
        std::cout << std::string(indent, ' ') << "Limit " << limit_count;
        std::cout << " (cost: " << estimated_cost << ", rows: " << estimated_cardinality << ")\n";
        if (child) child->explain(indent + 2);
    }
};

// Execution Plan class
class ExecutionPlan {
private:
    std::unique_ptr<PlanNode> root_;
    double total_cost_ = 0.0;
    size_t total_cardinality_ = 0;
    std::vector<std::string> used_indexes_;
    std::string original_query_;

public:
    ExecutionPlan() = default;
    explicit ExecutionPlan(std::unique_ptr<PlanNode> root) : root_(std::move(root)) {
        if (root_) {
            total_cost_ = root_->estimated_cost;
            total_cardinality_ = root_->estimated_cardinality;
        }
    }

    // Move constructor
    ExecutionPlan(ExecutionPlan&& other) noexcept
        : root_(std::move(other.root_)),
          total_cost_(other.total_cost_),
          total_cardinality_(other.total_cardinality_),
          used_indexes_(std::move(other.used_indexes_)),
          original_query_(std::move(other.original_query_)) {}

    // Move assignment
    ExecutionPlan& operator=(ExecutionPlan&& other) noexcept {
        if (this != &other) {
            root_ = std::move(other.root_);
            total_cost_ = other.total_cost_;
            total_cardinality_ = other.total_cardinality_;
            used_indexes_ = std::move(other.used_indexes_);
            original_query_ = std::move(other.original_query_);
        }
        return *this;
    }

    // Getters
    double getCost() const { return total_cost_; }
    size_t getCardinality() const { return total_cardinality_; }
    const std::vector<std::string>& getUsedIndexes() const { return used_indexes_; }
    const PlanNode* getRoot() const { return root_.get(); }
    std::string getOriginalQuery() const { return original_query_; }

    // Setters
    void setCost(double cost) { total_cost_ = cost; }
    void setCardinality(size_t card) { total_cardinality_ = card; }
    void addUsedIndex(const std::string& index) { used_indexes_.push_back(index); }
    void setOriginalQuery(const std::string& query) { original_query_ = query; }

    // Explain plan
    void explain() const {
        std::cout << "Execution Plan (Total Cost: " << total_cost_
                  << ", Estimated Rows: " << total_cardinality_ << ")\n";
        if (root_) {
            try {
                root_->explain(2);
            } catch (const std::exception& e) {
                std::cout << "  <error explaining plan: " << e.what() << ">\n";
            } catch (...) {
                std::cout << "  <error explaining plan>\n";
            }
        } else {
            std::cout << "  <empty plan>\n";
        }
    }

    // Comparison operators
    bool operator<(const ExecutionPlan& other) const {
        return total_cost_ < other.total_cost_;
    }

    bool operator==(const ExecutionPlan& other) const {
        return total_cost_ == other.total_cost_;
    }
};

} // namespace sqlopt
