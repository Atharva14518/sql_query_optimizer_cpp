#include "mysql_connector.h"
#include <iostream>
#include <sstream>

namespace sqlopt {

MySQLConnector::MySQLConnector() : mysql_(nullptr), connected_(false) {
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        throw std::runtime_error("Failed to initialize MySQL client");
    }
}

MySQLConnector::~MySQLConnector() {
    disconnect();
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

bool MySQLConnector::connect(const std::string& host, const std::string& user,
                           const std::string& password, const std::string& database,
                           unsigned int port) {
    if (!mysql_) return false;

    if (mysql_real_connect(mysql_, host.c_str(), user.c_str(), password.c_str(),
                          database.empty() ? nullptr : database.c_str(), port, nullptr, 0)) {
        connected_ = true;
        return true;
    }

    std::cerr << "MySQL connection failed: " << mysql_error(mysql_) << std::endl;
    return false;
}

void MySQLConnector::disconnect() {
    if (connected_) {
        mysql_close(mysql_);
        mysql_ = mysql_init(nullptr);
        connected_ = false;
    }
}

bool MySQLConnector::isConnected() const {
    return connected_;
}

std::vector<std::string> MySQLConnector::getDatabases() {
    std::vector<std::string> databases;

    if (!connected_) return databases;

    QueryResult result = executeQuery("SHOW DATABASES");
    if (result.success) {
        for (const auto& row : result.rows) {
            if (!row.empty()) {
                databases.push_back(row[0]);
            }
        }
    }

    return databases;
}

bool MySQLConnector::selectDatabase(const std::string& database) {
    if (!connected_) return false;

    if (mysql_select_db(mysql_, database.c_str()) == 0) {
        return true;
    }

    std::cerr << "Failed to select database: " << mysql_error(mysql_) << std::endl;
    return false;
}

MySQLConnector::QueryResult MySQLConnector::executeQuery(const std::string& sql) {
    QueryResult result;
    result.success = false;

    if (!connected_) {
        result.error_message = "Not connected to database";
        return result;
    }

    if (mysql_query(mysql_, sql.c_str()) != 0) {
        result.error_message = mysql_error(mysql_);
        return result;
    }

    MYSQL_RES* mysql_result = mysql_store_result(mysql_);
    if (!mysql_result) {
        // Query was not a SELECT
        result.affected_rows = mysql_affected_rows(mysql_);
        result.success = true;
        return result;
    }

    // Process SELECT result
    unsigned int num_fields = mysql_num_fields(mysql_result);
    MYSQL_FIELD* fields = mysql_fetch_fields(mysql_result);

    // Get column names
    for (unsigned int i = 0; i < num_fields; ++i) {
        result.columns.push_back(fields[i].name);
    }

    // Get rows
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(mysql_result))) {
        result.rows.push_back(fetchRow(row, num_fields));
    }

    freeResult(mysql_result);
    result.success = true;
    return result;
}

bool MySQLConnector::executeStatement(const std::string& sql) {
    if (!connected_) return false;

    if (mysql_query(mysql_, sql.c_str()) == 0) {
        return true;
    }

    std::cerr << "Query execution failed: " << mysql_error(mysql_) << std::endl;
    return false;
}

std::vector<MySQLConnector::TableInfo> MySQLConnector::getTables() {
    std::vector<TableInfo> tables;

    QueryResult result = executeQuery("SHOW TABLES");
    if (!result.success) return tables;

    for (const auto& row : result.rows) {
        if (!row.empty()) {
            TableInfo info = getTableInfo(row[0]);
            if (info.row_count >= 0) { // Valid table
                tables.push_back(info);
            }
        }
    }

    return tables;
}

MySQLConnector::TableInfo MySQLConnector::getTableInfo(const std::string& table_name) {
    TableInfo info;
    info.name = table_name;
    info.row_count = -1;

    // Get row count
    std::string count_sql = "SELECT COUNT(*) FROM `" + table_name + "`";
    QueryResult count_result = executeQuery(count_sql);
    if (count_result.success && !count_result.rows.empty() && !count_result.rows[0].empty()) {
        info.row_count = std::stoll(count_result.rows[0][0]);
    }

    // Get columns
    std::string desc_sql = "DESCRIBE `" + table_name + "`";
    QueryResult desc_result = executeQuery(desc_sql);
    if (desc_result.success) {
        for (const auto& row : desc_result.rows) {
            if (row.size() >= 2) {
                info.columns.push_back(row[0]);
                info.column_types[row[0]] = row[1];
            }
        }
    }

    // Get indexes
    std::string index_sql = "SHOW INDEX FROM `" + table_name + "`";
    QueryResult index_result = executeQuery(index_sql);
    if (index_result.success) {
        for (const auto& row : index_result.rows) {
            if (row.size() >= 5 && row[2] == "PRIMARY") {
                info.indexes.push_back("PRIMARY on (" + row[4] + ")");
            } else if (row.size() >= 5) {
                info.indexes.push_back(row[2] + " on (" + row[4] + ")");
            }
        }
    }

    return info;
}

std::vector<MySQLConnector::ColumnStats> MySQLConnector::getColumnStats(const std::string& table_name) {
    std::vector<ColumnStats> stats;

    TableInfo table_info = getTableInfo(table_name);
    if (table_info.row_count <= 0) return stats;

    for (const auto& column : table_info.columns) {
        ColumnStats col_stat;
        col_stat.name = column;

        // Get distinct count
        std::string distinct_sql = "SELECT COUNT(DISTINCT `" + column + "`) FROM `" + table_name + "`";
        QueryResult distinct_result = executeQuery(distinct_sql);
        if (distinct_result.success && !distinct_result.rows.empty() && !distinct_result.rows[0].empty()) {
            col_stat.distinct_count = std::stoll(distinct_result.rows[0][0]);
            col_stat.selectivity = static_cast<double>(col_stat.distinct_count) / table_info.row_count;
        } else {
            col_stat.distinct_count = 0;
            col_stat.selectivity = 0.1; // Default selectivity
        }

        stats.push_back(col_stat);
    }

    return stats;
}

void MySQLConnector::freeResult(MYSQL_RES* result) {
    if (result) {
        mysql_free_result(result);
    }
}

std::vector<std::string> MySQLConnector::fetchRow(MYSQL_ROW row, unsigned int num_fields) {
    std::vector<std::string> result_row;
    for (unsigned int i = 0; i < num_fields; ++i) {
        result_row.push_back(row[i] ? row[i] : "NULL");
    }
    return result_row;
}

} // namespace sqlopt  
