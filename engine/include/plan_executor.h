#pragma once
#include <memory>
#include <string>
#include <vector>
#include "execution_plan.h"
#include "mysql_connector.h"

namespace sqlopt {

class PlanExecutor {
public:
    explicit PlanExecutor(std::shared_ptr<MySQLConnector> connector);
    ~PlanExecutor() = default;

    // Execute a plan and return results
    struct ExecutionResult {
        std::vector<std::vector<std::string>> rows;
        std::vector<std::string> columns;
        long long execution_time_ms;
        size_t rows_affected;
        std::string error_message;
        bool success;
    };

    ExecutionResult execute(const ExecutionPlan& plan);

    // Execute raw SQL for comparison
    ExecutionResult executeRawSQL(const std::string& sql);

private:
    std::shared_ptr<MySQLConnector> connector_;

    // Helper methods for different plan types
    ExecutionResult executeTableScan(const ScanNode& node);
    ExecutionResult executeIndexScan(const IndexScanNode& node);
    ExecutionResult executeJoin(const JoinNode& node);
    ExecutionResult executeSort(const SortNode& node);
    ExecutionResult executeLimit(const LimitNode& node);

    std::string planToSQL(const ExecutionPlan& plan) const;
};

} // namespace sqlopt 
