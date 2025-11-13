#include "config.h"

namespace sqlopt {

Config::Config() {
    // Set default values
    config_["mysql_host"] = std::string("localhost");
    config_["mysql_user"] = std::string("root");
    config_["mysql_password"] = std::string("");
    config_["log_level"] = std::string("INFO");
    config_["log_file"] = std::string("sqlopt.log");
    config_["max_join_tables"] = 10;
    config_["enable_genetic_optimization"] = false;
    config_["benchmark_iterations"] = 5;
}

std::string Config::getString(const std::string& key, const std::string& default_val) const {
    auto it = config_.find(key);
    if (it != config_.end() && std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    return default_val;
}

int Config::getInt(const std::string& key, int default_val) const {
    auto it = config_.find(key);
    if (it != config_.end() && std::holds_alternative<int>(it->second)) {
        return std::get<int>(it->second);
    }
    return default_val;
}

double Config::getDouble(const std::string& key, double default_val) const {
    auto it = config_.find(key);
    if (it != config_.end() && std::holds_alternative<double>(it->second)) {
        return std::get<double>(it->second);
    }
    return default_val;
}

bool Config::getBool(const std::string& key, bool default_val) const {
    auto it = config_.find(key);
    if (it != config_.end() && std::holds_alternative<bool>(it->second)) {
        return std::get<bool>(it->second);
    }
    return default_val;
}

void Config::setString(const std::string& key, const std::string& value) {
    config_[key] = value;
}

void Config::setInt(const std::string& key, int value) {
    config_[key] = value;
}

void Config::setDouble(const std::string& key, double value) {
    config_[key] = value;
}

void Config::setBool(const std::string& key, bool value) {
    config_[key] = value;
}

} // namespace sqlopt
