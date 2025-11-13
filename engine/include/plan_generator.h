#pragma once
#include "statistics_manager.h"
#include "cost_estimator.h"
#include "execution_plan.h"
#include "ast.h"
#include <vector>
#include <memory>

namespace sqlopt {

class PlanGenerator {
private:
    std::shared_ptr<StatisticsManager> stats_mgr_;
    std::shared_ptr<CostEstimator> cost_estimator_;

    // Generate scan plans for a table
    std::vector<std::unique_ptr<PlanNode>> generateScanPlans(const std::string& table_name,
                                                            const std::string& alias = "");

    // Generate join plans using dynamic programming
    std::vector<std::unique_ptr<PlanNode>> generateJoinPlans(
        const std::vector<std::string>& tables,
        const std::vector<std::vector<std::string>>& join_conditions);

    // Generate filter plans
    std::unique_ptr<PlanNode> generateFilterPlan(std::unique_ptr<PlanNode> child,
                                                const std::vector<std::string>& conditions);

    // Generate sort plans
    std::unique_ptr<PlanNode> generateSortPlan(std::unique_ptr<PlanNode> child,
                                              const std::vector<OrderItem>& order_by);

    // Generate aggregate plans
    std::unique_ptr<PlanNode> generateAggregatePlan(std::unique_ptr<PlanNode> child,
                                                   const std::vector<std::string>& group_by,
                                                   const std::vector<std::string>& aggregates);

    // Generate limit plans
    std::unique_ptr<PlanNode> generateLimitPlan(std::unique_ptr<PlanNode> child, size_t limit);

    // Estimate costs for a plan
    void estimatePlanCosts(PlanNode* node);

public:
    PlanGenerator(std::shared_ptr<StatisticsManager> stats, std::shared_ptr<CostEstimator> cost_est)
        : stats_mgr_(std::move(stats)), cost_estimator_(std::move(cost_est)) {}

    // Generate all possible execution plans for a SELECT query
    std::vector<ExecutionPlan> generatePlans(const SelectQuery& query);

    // Get the best plan (lowest cost)
    ExecutionPlan getBestPlan(std::vector<ExecutionPlan>& plans);

    // Generate left-deep join tree
    std::unique_ptr<PlanNode> generateLeftDeepJoin(
        const std::vector<std::string>& tables,
        const std::vector<std::vector<std::string>>& conditions);

    // Generate bushy join tree (more complex)
    std::unique_ptr<PlanNode> generateBushyJoin(
        const std::vector<std::string>& tables,
        const std::vector<std::vector<std::string>>& conditions);

}; // class PlanGenerator

} // namespace sqlopt
