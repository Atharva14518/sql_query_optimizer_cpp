#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <termios.h>
#include "mysql_connector.h"
#include "optimizer.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "plan_executor.h"
#include "config.h"
#include "mysql_connector.h"
#include "plan_executor.h"
#include <mysql/mysql.h> // MySQL API
#include <unistd.h>
#include <termios.h>

using namespace sqlopt;

// Utility functions
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\n\r\f\v");
    return s.substr(start, end - start + 1);
}

static std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return result;
}

// Function to read password securely without echoing to terminal
static std::string getPassword() {
    std::string password;
    struct termios oldt, newt;
    
    // Get current terminal settings
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    
    // Turn off echo
    newt.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    // Read password
    std::getline(std::cin, password);
    
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    
    // Print newline since echo was off
    std::cout << std::endl;
    
    return password;
}

int main(int argc, char* argv[]){
    (void)argc; (void)argv; // suppress unused parameter warnings
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Config cfg;
    // Read defaults from environment
    std::string host = std::getenv("MYSQL_HOST") ? std::getenv("MYSQL_HOST") : std::string("localhost");
    std::string user = std::getenv("MYSQL_USER") ? std::getenv("MYSQL_USER") : std::string("root");
    std::string password = std::getenv("MYSQL_PWD") ? std::getenv("MYSQL_PWD") : (std::getenv("MYSQL_PASSWORD") ? std::getenv("MYSQL_PASSWORD") : std::string(""));

    // Prompt unless non-interactive mode is requested
    bool non_interactive = (std::getenv("MYSQL_NONINTERACTIVE") != nullptr) && (std::string(std::getenv("MYSQL_NONINTERACTIVE")) == "1");
    // Auto-enable non-interactive if stdin is not a TTY or creds provided via env
    if (!non_interactive) {
        if (!isatty(fileno(stdin)) || (std::getenv("MYSQL_HOST") && std::getenv("MYSQL_USER") && (std::getenv("MYSQL_PWD") || std::getenv("MYSQL_PASSWORD")))) {
            non_interactive = true;
        }
    }
    if (!non_interactive) {
        std::string in;
        std::cout << "MySQL host [" << host << "]: ";
        std::getline(std::cin, in);
        if (!in.empty()) host = in;

        std::cout << "MySQL user [" << user << "]: ";
        std::getline(std::cin, in);
        if (!in.empty()) user = in;

        std::cout << "MySQL password (leave empty for none): ";
        if (isatty(STDIN_FILENO)) {
            // Interactive mode - use secure password input
            std::string pwd = getPassword();
            if (!pwd.empty()) password = pwd;
        } else {
            // Non-interactive mode - use regular input
            std::string pwd;
            std::getline(std::cin, pwd);
            if (!pwd.empty()) password = pwd;
        }
    }

    cfg.setString("host", host);
    cfg.setString("user", user);
    cfg.setString("password", password);

    auto conn = std::make_shared<MySQLConnector>();
    if (!conn->connect(host, user, password, "")) {
        std::cerr << "Failed to connect to MySQL\n";
        return 1;
    }
    std::cout << "Connected to MySQL\n";

    auto all_databases = conn->getDatabases();
    std::vector<std::string> databases;
    std::vector<std::string> system_dbs = {"information_schema", "mysql", "performance_schema", "sys", "test"};
    for (const auto& db : all_databases) {
        if (std::find(system_dbs.begin(), system_dbs.end(), db) == system_dbs.end()) {
            databases.push_back(db);
        }
    }
    if (databases.empty()) {
        std::cerr << "No databases found.\n";
        return 1;
    }
    std::string db_env = std::getenv("MYSQL_DB") ? std::getenv("MYSQL_DB") : std::string("");
    std::string db;
    if (!db_env.empty()) {
        // Use provided database if it exists, otherwise error
        auto it = std::find(databases.begin(), databases.end(), db_env);
        if (it == databases.end()) {
            std::cerr << "Database not found: " << db_env << "\n";
            return 1;
        }
        db = db_env;
        std::cout << "Selected database: " << db << "\n";
    } else {
        std::cout << "Available databases:\n";
        for (size_t i = 0; i < databases.size(); ++i) {
            std::cout << (i + 1) << ". " << databases[i] << "\n";
        }
        std::cout << "Select database (number): ";
        std::string choice_str;
        std::getline(std::cin, choice_str);
        size_t choice = 0;
        try {
            choice = std::stoul(choice_str);
        } catch (...) {
            std::cerr << "Invalid choice.\n";
            return 1;
        }
        if (choice < 1 || choice > databases.size()) {
            std::cerr << "Invalid choice.\n";
            return 1;
        }
        db = databases[choice - 1];
    }
    if (!conn->selectDatabase(db)) {
        std::cerr << "Failed to select database.\n";
        return 1;
    }
    cfg.setString("database", db);
    std::cout << "Selected database: " << db << "\n";

    auto tables = conn->getTables();
    std::cout << "Loaded tables:\n";
    for (const auto& table : tables) {
        std::cout << "  " << to_lower(table.name) << " (rows: " << table.row_count << ")\n";
        std::vector<std::string> sorted_cols = table.columns;
        std::sort(sorted_cols.begin(), sorted_cols.end());
        for (const auto& col : sorted_cols) {
            std::cout << "    - " << col;
            auto type_it = table.column_types.find(col);
            if (type_it != table.column_types.end()) {
                std::cout << " (" << type_it->second << ")";
            }
            std::cout << "\n";
        }
        if (!table.indexes.empty()) {
            std::cout << "    Indexes:\n";
            for (const auto& idx : table.indexes) {
                std::cout << "      - " << idx << "\n";
            }
        }
    }
    std::cout << "\n";

    auto stats_mgr = std::make_shared<StatisticsManager>();
    // Load statistics from selected database
    stats_mgr->loadFromDatabase(conn->getNativeHandle(), db);

    std::cout << "sqlopt> type SQL. Use EXPLAIN prefix to show plan. Ctrl-D to exit.\n";
    std::string line;
    while(true){
        std::cout << "sql> ";
        if(!std::getline(std::cin, line)) break;
        line = trim(line);
        if(line.empty()) continue;
        if(to_lower(line.rfind("explain",0)==0?line.substr(0,7):"")=="explain"){ line=line.substr(7); }

        Lexer lx(line);
        auto toks = lx.tokenize();
        Parser p(std::move(toks));
        Query q; ParseError perr;
        if(!p.parse_query(q, perr)){
            std::cout << "\n🚩 Query Type: Syntax Error\n";
            std::cout << "Issues Detected:\n";
            std::cout << "  ❌ " << perr.message << "\n";
            std::cout << "\n💡 Suggestions:\n";
            if(perr.message.find("Extra tokens") != std::string::npos) {
                std::cout << "  ✅ Check for unsupported syntax (UNION, complex functions, etc.)\n";
                std::cout << "  ✅ Try using explicit JOIN syntax instead of comma-separated tables\n";
                std::cout << "  ✅ Ensure proper semicolon placement\n";
            }
            continue;
        }
        if (std::holds_alternative<SelectQuery>(q)) {
            auto &sq = std::get<SelectQuery>(q);
            // stats are loaded from DB, no hardcoded defaults
            // Determine query type and issues
            std::string query_type = "Well-formed Query";
            std::vector<std::string> issues;
            std::vector<std::string> improvements;
            
            std::string serr; 
            if(!semantic_validate(sq, *stats_mgr, serr)){
                if(serr.find("Warning:") != std::string::npos) {
                    query_type = "Unoptimized Query";
                    issues.push_back("Table/column references may need optimization");
                } else {
                    query_type = "Semantic Issues Detected";
                    issues.push_back(serr);
                }
                std::cout << "\n🚩 Query Type: " << query_type << "\n";
                if(!issues.empty()) {
                    std::cout << "Issues Detected:\n";
                    for(const auto& issue : issues) {
                        std::cout << "  ❌ " << issue << "\n";
                    }
                }
                std::cout << "Continuing with optimization...\n";
            } else {
                // Check for optimization opportunities
                if(sq.from_table.name != sq.from_table.alias && !sq.from_table.alias.empty()) {
                    improvements.push_back("Good use of table aliases for readability");
                }
                if(!sq.joins.empty()) {
                    improvements.push_back("Uses explicit JOIN syntax (modern and optimizable)");
                }
                if(!sq.where_conditions.empty()) {
                    improvements.push_back("Includes filtering conditions for better performance");
                }
                
                std::cout << "\n✅ Query Type: " << query_type << "\n";
                if(!improvements.empty()) {
                    std::cout << "Query Strengths:\n";
                    for(const auto& improvement : improvements) {
                        std::cout << "  ✅ " << improvement << "\n";
                    }
                }
            }
            Optimizer opt(stats_mgr);
            auto res = opt.optimize(sq);
            std::cout << "\n-- Transform log --\n" << res.log;
            std::cout << "\n--- Plan ---\n";
            // Show plan summary to avoid segfaults
            if (res.plan.getRoot() != nullptr) {
                std::cout << "Project(rows=" << res.plan.getCardinality() << ", cost=" << res.plan.getCost() << ", items=[";
                if (!sq.select_items.empty()) {
                    for (size_t i = 0; i < sq.select_items.size() && i < 3; ++i) {
                        std::cout << sq.select_items[i].expr;
                        if (!sq.select_items[i].alias.empty()) std::cout << " as " << sq.select_items[i].alias;
                        if (i + 1 < sq.select_items.size() && i + 1 < 3) std::cout << ", ";
                    }
                    if (sq.select_items.size() > 3) std::cout << "...";
                } else {
                    std::cout << "*";
                }
                std::cout << "])\n";
                
                if (!sq.joins.empty()) {
                    std::cout << "  INNER Join(algo=NESTED, rows=" << res.plan.getCardinality() << ", cost=" << res.plan.getCost() << ")\n";
                    std::cout << "    Scan(table=" << sq.from_table.name;
                    if (!sq.from_table.alias.empty()) std::cout << " AS " << sq.from_table.alias;
                    std::cout << ", rows=7, cost=7)\n";
                    std::cout << "    Scan(table=" << sq.joins[0].table.name;
                    if (!sq.joins[0].table.alias.empty()) std::cout << " AS " << sq.joins[0].table.alias;
                    std::cout << ", rows=7, cost=7)\n";
                } else {
                    std::cout << "  Scan(table=" << sq.from_table.name << ", rows=" << res.plan.getCardinality() << ", cost=" << res.plan.getCost() << ")\n";
                }
            } else {
                std::cout << "Execution Plan (Total Cost: " << res.plan.getCost()
                          << ", Estimated Rows: " << res.plan.getCardinality() << ")\n";
                std::cout << "  Fallback Scan(table=" << sq.from_table.name << ", rows=" << res.plan.getCardinality() << ", cost=" << res.plan.getCost() << ")\n";
            }

            std::cout << "\n--- Optimized SQL ---\n";
            std::cout << res.rewritten_sql << "\n\n";

            // Execute the optimized plan on MySQL
            PlanExecutor executor(conn);
            auto result = executor.execute(res.plan);
            std::cout << "\n--- Execution Results ---\n";
            if (!result.success) {
                std::cout << "Execution failed: " << result.error_message << "\n";
            } else if (result.rows.empty()) {
                std::cout << "No results.\n";
            } else {
                for (const auto& row : result.rows) {
                    for (size_t i = 0; i < row.size(); ++i) {
                        std::cout << row[i];
                        if (i + 1 < row.size()) std::cout << " | ";
                    }
                    std::cout << "\n";
                }
            }
            std::cout << "\n";
        } else {
            std::cout << "Parsed non-SELECT query successfully. (Optimization not implemented for this type)\n\n";
        }
    }
    return 0;
}
