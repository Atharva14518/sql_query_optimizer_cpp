#include "plan_executor.h"
#include <iostream>
#include <sstream>
#include <chrono>

namespace sqlopt {

PlanExecutor::PlanExecutor(std::shared_ptr<MySQLConnector> connector)
    : connector_(connector) {}

PlanExecutor::ExecutionResult PlanExecutor::execute(const ExecutionPlan& plan) {
    ExecutionResult result;
    result.success = false;

    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        // For now, convert the plan back to SQL and execute it
        // In a full implementation, this would execute each node in the plan tree
        std::string sql = planToSQL(plan);
        result = executeRawSQL(sql);
    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    return result;
}

PlanExecutor::ExecutionResult PlanExecutor::executeRawSQL(const std::string& sql) {
    ExecutionResult result;

    auto start_time = std::chrono::high_resolution_clock::now();

    MySQLConnector::QueryResult mysql_result = connector_->executeQuery(sql);

    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    result.success = mysql_result.success;
    result.rows = mysql_result.rows;
    result.columns = mysql_result.columns;
    result.rows_affected = mysql_result.affected_rows;
    result.error_message = mysql_result.error_message;

    return result;
}

std::string PlanExecutor::planToSQL(const ExecutionPlan& plan) const {
    // Simple conversion back to SQL - in a full implementation,
    // this would traverse the plan tree and generate appropriate SQL
    std::stringstream sql;

    // For now, just return the original query
    // TODO: Implement proper plan-to-SQL conversion
    sql << plan.getOriginalQuery();

    return sql.str();
}

} // namespace sqlopt
