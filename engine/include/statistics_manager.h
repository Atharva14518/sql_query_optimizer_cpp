#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace sqlopt {

struct ColumnStats {
    std::string column_name;
    size_t distinct_values = 0;
    std::string min_value;
    std::string max_value;
    double selectivity = 0.1; // Default selectivity
    std::vector<std::pair<std::string, double>> histogram; // value -> frequency
};

struct IndexInfo {
    std::string index_name;
    std::vector<std::string> columns;
    bool is_unique = false;
    size_t cardinality = 0;
};

struct TableStatistics {
    std::string table_name;
    size_t row_count = 0;
    size_t page_count = 0;
    std::map<std::string, ColumnStats> column_stats;
    std::vector<IndexInfo> available_indexes;
};

class StatisticsManager {
private:
    std::map<std::string, TableStatistics> table_stats_;
    static constexpr size_t HISTOGRAM_BUCKETS = 10;

public:
    StatisticsManager() = default;

    // Load statistics from database
    void loadFromDatabase(void* mysql_conn, const std::string& db_name);

    // Get table statistics
    const TableStatistics* getTableStats(const std::string& table_name) const;

    // Case-insensitive table lookup helpers
    const TableStatistics* getTableStatsCI(const std::string& table_name) const;
    std::string resolveTableNameCI(const std::string& table_name) const;

    // Estimate selectivity for a condition
    double estimateSelectivity(const std::string& table_name, const std::string& column,
                              const std::string& op, const std::string& value) const;

    // Get row count estimate
    size_t estimateRowCount(const std::string& table_name, double selectivity) const;

    // Update statistics
    void updateTableStats(const std::string& table_name, const TableStatistics& stats);

    // Build histogram for a column
    void buildHistogram(ColumnStats& col_stats, const std::vector<std::string>& values);

    // Print statistics
    void printStats() const;
};

} // namespace sqlopt
