#pragma once
#include "statistics_manager.h"
#include <memory>

namespace sqlopt {

struct CostComponents {
    double io_cost = 0.0;      // I/O operations cost
    double cpu_cost = 0.0;     // CPU processing cost
    double memory_cost = 0.0;  // Memory usage cost
    double network_cost = 0.0; // Network cost (for distributed)

    double total() const { return io_cost + cpu_cost + memory_cost + network_cost; }

    CostComponents& operator+=(const CostComponents& other) {
        io_cost += other.io_cost;
        cpu_cost += other.cpu_cost;
        memory_cost += other.memory_cost;
        network_cost += other.network_cost;
        return *this;
    }
};

class CostEstimator {
private:
    std::shared_ptr<StatisticsManager> stats_mgr_;

    // Cost constants (can be tuned)
    static constexpr double SEQ_PAGE_COST = 1.0;
    static constexpr double RAND_PAGE_COST = 4.0;
    static constexpr double CPU_TUPLE_COST = 0.01;
    static constexpr double INDEX_LOOKUP_COST = 2.0;
    static constexpr double SORT_COST_PER_TUPLE = 0.1;

public:
    explicit CostEstimator(std::shared_ptr<StatisticsManager> stats_mgr)
        : stats_mgr_(std::move(stats_mgr)) {}

    // Table scan cost
    CostComponents estimateTableScan(const std::string& table_name, double selectivity = 1.0);

    // Index scan cost
    CostComponents estimateIndexScan(const std::string& table_name, const std::string& index_col,
                                   double selectivity = 1.0);

    // Join cost estimation
    CostComponents estimateJoinCost(size_t left_rows, size_t right_rows,
                                  const std::string& join_type = "nested_loop");

    // Sort cost
    CostComponents estimateSortCost(size_t num_tuples, size_t num_columns);

    // Aggregation cost
    CostComponents estimateAggregationCost(size_t input_rows, size_t group_by_cols);

    // Filter cost
    CostComponents estimateFilterCost(size_t input_rows, double selectivity);

    // Combined operation cost
    CostComponents estimateQueryCost(const std::vector<std::string>& operations,
                                   const std::vector<size_t>& cardinalities);

    // Utility functions
    double getPageCount(const std::string& table_name) const;
    double getRowCount(const std::string& table_name) const;
};

} // namespace sqlopt
