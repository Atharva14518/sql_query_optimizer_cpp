#pragma once
#include <memory>
#include <string>
#include <vector>
#include "ast.h"
#include "execution_plan.h"
#include "statistics_manager.h"
#include "cost_estimator.h"
#include "plan_generator.h"
#include "query_rewriter.h"

namespace sqlopt {

struct OptimizeResult {
    ExecutionPlan plan;
    std::string log;
    std::string rewritten_sql;
};

class Optimizer {
    std::shared_ptr<StatisticsManager> stats_mgr_;
    std::shared_ptr<CostEstimator> cost_estimator_;
    std::shared_ptr<PlanGenerator> plan_generator_;
    QueryRewriter rewriter_;

public:
    explicit Optimizer(std::shared_ptr<StatisticsManager> stats_mgr);
    OptimizeResult optimize(const SelectQuery& q);
};

} // namespace sqlopt
