#include "cost_estimator.h"
#include <cmath>
#include <algorithm>

namespace sqlopt {

CostComponents CostEstimator::estimateTableScan(const std::string& table_name, double selectivity) {
    CostComponents cost;

    const TableStatistics* ts = stats_mgr_->getTableStats(table_name);
    if (!ts) return cost;

    size_t pages_to_read = static_cast<size_t>(ts->page_count * selectivity);
    if (pages_to_read == 0) pages_to_read = 1;

    // I/O cost: sequential page reads
    cost.io_cost = pages_to_read * SEQ_PAGE_COST;

    // CPU cost: process tuples
    size_t tuples_to_process = static_cast<size_t>(ts->row_count * selectivity);
    cost.cpu_cost = tuples_to_process * CPU_TUPLE_COST;

    return cost;
}

CostComponents CostEstimator::estimateIndexScan(const std::string& table_name, const std::string& index_col [[maybe_unused]],
                                              double selectivity) {
    CostComponents cost;

    const TableStatistics* ts = stats_mgr_->getTableStats(table_name);
    if (!ts) return cost;

    // Index lookup cost
    cost.io_cost = INDEX_LOOKUP_COST;

    // Data page access (random I/O for index scan)
    size_t data_pages = static_cast<size_t>(ts->page_count * selectivity);
    if (data_pages == 0) data_pages = 1;
    cost.io_cost += data_pages * RAND_PAGE_COST;

    // CPU cost
    size_t tuples = static_cast<size_t>(ts->row_count * selectivity);
    cost.cpu_cost = tuples * CPU_TUPLE_COST;

    return cost;
}

CostComponents CostEstimator::estimateJoinCost(size_t left_rows, size_t right_rows,
                                             const std::string& join_type) {
    CostComponents cost;

    if (join_type == "nested_loop") {
        // Nested loop join: O(left_rows * right_rows)
        cost.cpu_cost = left_rows * right_rows * CPU_TUPLE_COST;
        // I/O cost depends on buffer management, simplified
        cost.io_cost = (left_rows + right_rows) * SEQ_PAGE_COST;
    } else if (join_type == "hash_join") {
        // Hash join: build hash table + probe
        cost.cpu_cost = (left_rows + right_rows) * CPU_TUPLE_COST * 2;
        cost.memory_cost = std::max(left_rows, right_rows) * 0.1; // Memory for hash table
        cost.io_cost = (left_rows + right_rows) * SEQ_PAGE_COST;
    } else if (join_type == "merge_join") {
        // Merge join: requires sorted inputs
        cost.cpu_cost = (left_rows + right_rows) * CPU_TUPLE_COST;
        cost.io_cost = (left_rows + right_rows) * SEQ_PAGE_COST;
    }

    return cost;
}

CostComponents CostEstimator::estimateSortCost(size_t num_tuples, size_t num_columns) {
    CostComponents cost;

    // External sort cost estimation (simplified)
    // Assume 2-phase external sort
    double sort_passes = std::log2(num_tuples) / std::log2(1000); // Assuming 1000 tuples per page
    cost.io_cost = num_tuples * sort_passes * RAND_PAGE_COST;

    // CPU cost for comparisons
    cost.cpu_cost = num_tuples * std::log2(num_tuples) * num_columns * CPU_TUPLE_COST;

    return cost;
}

CostComponents CostEstimator::estimateAggregationCost(size_t input_rows, size_t group_by_cols) {
    CostComponents cost;

    // CPU cost for grouping and aggregation
    cost.cpu_cost = input_rows * group_by_cols * CPU_TUPLE_COST;

    // Memory cost for group-by hash table
    cost.memory_cost = input_rows * 0.1; // Estimate

    return cost;
}

CostComponents CostEstimator::estimateFilterCost(size_t input_rows, double selectivity) {
    CostComponents cost;

    // CPU cost for evaluating predicates
    cost.cpu_cost = input_rows * CPU_TUPLE_COST;

    // I/O cost (if filtering requires additional reads)
    size_t output_rows = static_cast<size_t>(input_rows * selectivity);
    cost.io_cost = output_rows * SEQ_PAGE_COST * 0.1; // Minimal additional I/O

    return cost;
}

CostComponents CostEstimator::estimateQueryCost(const std::vector<std::string>& operations,
                                              const std::vector<size_t>& cardinalities) {
    CostComponents total_cost;

    if (operations.size() != cardinalities.size()) return total_cost;

    for (size_t i = 0; i < operations.size(); ++i) {
        const std::string& op = operations[i];
        size_t cardinality = cardinalities[i];

        if (op == "scan") {
            total_cost += estimateTableScan("", 1.0); // Simplified
        } else if (op == "filter") {
            total_cost += estimateFilterCost(cardinality, 0.5);
        } else if (op == "join") {
            if (i > 0) {
                total_cost += estimateJoinCost(cardinalities[i-1], cardinality);
            }
        } else if (op == "sort") {
            total_cost += estimateSortCost(cardinality, 1);
        } else if (op == "aggregate") {
            total_cost += estimateAggregationCost(cardinality, 1);
        }
    }

    return total_cost;
}

double CostEstimator::getPageCount(const std::string& table_name) const {
    const TableStatistics* ts = stats_mgr_->getTableStats(table_name);
    return ts ? ts->page_count : 0;
}

double CostEstimator::getRowCount(const std::string& table_name) const {
    const TableStatistics* ts = stats_mgr_->getTableStats(table_name);
    return ts ? ts->row_count : 0;
}

} // namespace sqlopt
