#pragma once
#include <mysql/mysql.h>
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace sqlopt {

class MySQLConnector {
public:
    MySQLConnector();
    ~MySQLConnector();

    // Connection management
    bool connect(const std::string& host, const std::string& user,
                const std::string& password, const std::string& database = "",
                unsigned int port = 3306);
    void disconnect();
    bool isConnected() const;

    // Database operations
    std::vector<std::string> getDatabases();
    bool selectDatabase(const std::string& database);

    // Query execution
    struct QueryResult {
        std::vector<std::vector<std::string>> rows;
        std::vector<std::string> columns;
        unsigned long long affected_rows;
        std::string error_message;
        bool success;
    };

    QueryResult executeQuery(const std::string& sql);
    bool executeStatement(const std::string& sql);

    // Schema information
    struct TableInfo {
        std::string name;
        unsigned long long row_count;
        std::vector<std::string> columns;
        std::map<std::string, std::string> column_types;
        std::vector<std::string> indexes;
    };

    std::vector<TableInfo> getTables();
    TableInfo getTableInfo(const std::string& table_name);

    // Statistics
    struct ColumnStats {
        std::string name;
        unsigned long long distinct_count;
        double selectivity;
    };

    std::vector<ColumnStats> getColumnStats(const std::string& table_name);

    // Access native handle (read-only usage expected)
    MYSQL* getNativeHandle() const { return mysql_; }

private:
    MYSQL* mysql_;
    bool connected_;

    // Helper methods
    void freeResult(MYSQL_RES* result);
    std::vector<std::string> fetchRow(MYSQL_ROW row, unsigned int num_fields);
};

} // namespace sqlopt
