#include "statistics_manager.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <mysql/mysql.h> 

namespace sqlopt {

void StatisticsManager::loadFromDatabase(void* mysql_conn, const std::string& db_name [[maybe_unused]]) {
    MYSQL* conn = static_cast<MYSQL*>(mysql_conn);
    if (!conn) return;

    // Get list of tables
    std::string query = "SHOW TABLES";
    if (mysql_query(conn, query.c_str()) != 0) {
        std::cerr << "Failed to get tables: " << mysql_error(conn) << std::endl;
        return;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row;
    std::vector<std::string> tables;

    while ((row = mysql_fetch_row(res))) {
        tables.push_back(row[0]);
    }
    mysql_free_result(res);

    // Load statistics for each table
    for (const auto& table : tables) {
        TableStatistics ts;
        ts.table_name = table;

        // Get row count
        query = "SELECT COUNT(*) FROM `" + table + "`";
        if (mysql_query(conn, query.c_str()) == 0) {
            MYSQL_RES* count_res = mysql_store_result(conn);
            MYSQL_ROW count_row = mysql_fetch_row(count_res);
            if (count_row) {
                ts.row_count = std::stoull(count_row[0]);
            }
            mysql_free_result(count_res);
        }

        // Estimate page count (rough estimate: 100 rows per page)
        ts.page_count = (ts.row_count + 99) / 100;

        // Get columns
        query = "DESCRIBE `" + table + "`";
        if (mysql_query(conn, query.c_str()) == 0) {
            MYSQL_RES* desc_res = mysql_store_result(conn);
            MYSQL_ROW desc_row;
            std::vector<std::string> columns;

            while ((desc_row = mysql_fetch_row(desc_res))) {
                columns.push_back(desc_row[0]);
            }
            mysql_free_result(desc_res);

            // Load column statistics
            for (const auto& col : columns) {
                ColumnStats cs;
                cs.column_name = col;

                // Get distinct values
                query = "SELECT COUNT(DISTINCT `" + col + "`) FROM `" + table + "`";
                if (mysql_query(conn, query.c_str()) == 0) {
                    MYSQL_RES* dist_res = mysql_store_result(conn);
                    MYSQL_ROW dist_row = mysql_fetch_row(dist_res);
                    if (dist_row) {
                        cs.distinct_values = std::stoull(dist_row[0]);
                    }
                    mysql_free_result(dist_res);
                }

                // Get min/max values
                query = "SELECT MIN(`" + col + "`), MAX(`" + col + "`) FROM `" + table + "`";
                if (mysql_query(conn, query.c_str()) == 0) {
                    MYSQL_RES* mm_res = mysql_store_result(conn);
                    MYSQL_ROW mm_row = mysql_fetch_row(mm_res);
                    if (mm_row) {
                        cs.min_value = mm_row[0] ? mm_row[0] : "";
                        cs.max_value = mm_row[1] ? mm_row[1] : "";
                    }
                    mysql_free_result(mm_res);
                }

                // Calculate selectivity
                if (ts.row_count > 0) {
                    cs.selectivity = static_cast<double>(cs.distinct_values) / ts.row_count;
                    if (cs.selectivity > 1.0) cs.selectivity = 1.0;
                }

                // Build histogram (sample values)
                if (cs.distinct_values > 0 && cs.distinct_values <= 1000) {
                    query = "SELECT `" + col + "`, COUNT(*) FROM `" + table +
                           "` GROUP BY `" + col + "` ORDER BY COUNT(*) DESC LIMIT 10";
                    if (mysql_query(conn, query.c_str()) == 0) {
                        MYSQL_RES* hist_res = mysql_store_result(conn);
                        MYSQL_ROW hist_row;
                        while ((hist_row = mysql_fetch_row(hist_res))) {
                            if (hist_row[0] && hist_row[1]) {
                                double freq = std::stod(hist_row[1]);
                                cs.histogram.emplace_back(hist_row[0], freq / ts.row_count);
                            }
                        }
                        mysql_free_result(hist_res);
                    }
                }

                ts.column_stats[col] = cs;
            }
        }

        // Get indexes
        query = "SHOW INDEX FROM `" + table + "`";
        if (mysql_query(conn, query.c_str()) == 0) {
            MYSQL_RES* idx_res = mysql_store_result(conn);
            MYSQL_ROW idx_row;
            std::map<std::string, IndexInfo> indexes;

            while ((idx_row = mysql_fetch_row(idx_res))) {
                std::string idx_name = idx_row[2];
                std::string col_name = idx_row[4];
                bool is_unique = (idx_row[1] && std::string(idx_row[1]) == "0");

                if (indexes.find(idx_name) == indexes.end()) {
                    indexes[idx_name] = {idx_name, {col_name}, is_unique, 0};
                } else {
                    indexes[idx_name].columns.push_back(col_name);
                }
            }
            mysql_free_result(idx_res);

            for (auto& idx : indexes) {
                ts.available_indexes.push_back(idx.second);
            }
        }

        table_stats_[table] = ts;
    }
}

const TableStatistics* StatisticsManager::getTableStats(const std::string& table_name) const {
    auto it = table_stats_.find(table_name);
    return it != table_stats_.end() ? &it->second : nullptr;
}

const TableStatistics* StatisticsManager::getTableStatsCI(const std::string& table_name) const {
    // exact match first
    auto it = table_stats_.find(table_name);
    if (it != table_stats_.end()) return &it->second;
    // case-insensitive search
    std::string target = table_name;
    std::transform(target.begin(), target.end(), target.begin(), [](unsigned char c){ return std::tolower(c); });
    for (const auto& kv : table_stats_) {
        std::string key = kv.first;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return std::tolower(c); });
        if (key == target) return &kv.second;
    }
    return nullptr;
}

std::string StatisticsManager::resolveTableNameCI(const std::string& table_name) const {
    auto it = table_stats_.find(table_name);
    if (it != table_stats_.end()) return it->first;
    std::string target = table_name;
    std::transform(target.begin(), target.end(), target.begin(), [](unsigned char c){ return std::tolower(c); });
    for (const auto& kv : table_stats_) {
        std::string key = kv.first;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return std::tolower(c); });
        if (key == target) return kv.first;
    }
    return table_name;
}

double StatisticsManager::estimateSelectivity(const std::string& table_name, const std::string& column,
                                             const std::string& op, const std::string& value) const {
    const TableStatistics* ts = getTableStats(table_name);
    if (!ts) return 0.1; // Default selectivity

    auto col_it = ts->column_stats.find(column);
    if (col_it == ts->column_stats.end()) return 0.1;

    const ColumnStats& cs = col_it->second;

    // Use histogram if available
    if (!cs.histogram.empty()) {
        for (const auto& bucket : cs.histogram) {
            if (bucket.first == value) {
                return bucket.second;
            }
        }
    }

    // Fallback to basic estimation
    if (op == "=") {
        return cs.selectivity;
    } else if (op == ">" || op == "<" || op == ">=" || op == "<=") {
        return 0.3; // Assume 30% for range queries
    } else if (op == "LIKE") {
        return 0.1; // Assume 10% for LIKE
    }

    return 0.1;
}

size_t StatisticsManager::estimateRowCount(const std::string& table_name, double selectivity) const {
    const TableStatistics* ts = getTableStats(table_name);
    if (!ts) return 0;
    return static_cast<size_t>(ts->row_count * selectivity);
}

void StatisticsManager::updateTableStats(const std::string& table_name, const TableStatistics& stats) {
    table_stats_[table_name] = stats;
}

void StatisticsManager::buildHistogram(ColumnStats& col_stats, const std::vector<std::string>& values) {
    if (values.empty()) return;

    std::map<std::string, size_t> freq;
    for (const auto& val : values) {
        freq[val]++;
    }

    col_stats.histogram.clear();
    for (const auto& p : freq) {
        col_stats.histogram.emplace_back(p.first, static_cast<double>(p.second) / values.size());
    }

    // Sort by frequency descending
    std::sort(col_stats.histogram.begin(), col_stats.histogram.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Keep only top buckets
    if (col_stats.histogram.size() > HISTOGRAM_BUCKETS) {
        col_stats.histogram.resize(HISTOGRAM_BUCKETS);
    }
}

void StatisticsManager::printStats() const {
    std::cout << "\n=== Database Statistics ===\n";
    for (const auto& p : table_stats_) {
        const TableStatistics& ts = p.second;
        std::cout << "Table: " << ts.table_name << " (rows: " << ts.row_count << ", pages: " << ts.page_count << ")\n";

        for (const auto& col_p : ts.column_stats) {
            const ColumnStats& cs = col_p.second;
            std::cout << "  Column: " << cs.column_name
                     << " (distinct: " << cs.distinct_values
                     << ", sel: " << cs.selectivity << ")\n";
        }

        if (!ts.available_indexes.empty()) {
            std::cout << "  Indexes:\n";
            for (const auto& idx : ts.available_indexes) {
                std::cout << "    " << idx.index_name << " on (";
                for (size_t i = 0; i < idx.columns.size(); ++i) {
                    std::cout << idx.columns[i];
                    if (i + 1 < idx.columns.size()) std::cout << ", ";
                }
                std::cout << ")\n";
            }
        }
        std::cout << std::endl;
    }
}

} // namespace sqlopt
