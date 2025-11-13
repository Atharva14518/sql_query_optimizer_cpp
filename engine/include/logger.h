#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <mutex>

namespace sqlopt {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
private:
    LogLevel level_;
    std::ofstream log_file_;
    std::mutex mutex_;
    bool console_output_;

    std::string levelToString(LogLevel level) const;

public:
    Logger(LogLevel level = LogLevel::INFO, const std::string& filename = "", bool console = true);
    ~Logger();

    void setLevel(LogLevel level);
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
};

} // namespace sqlopt
