#pragma once
#include <string>
#include <unordered_map>
#include <variant>

namespace sqlopt {

using ConfigValue = std::variant<std::string, int, double, bool>;

class Config {
private:
    std::unordered_map<std::string, ConfigValue> config_;

public:
    Config();

    // Getters
    std::string getString(const std::string& key, const std::string& default_val = "") const;
    int getInt(const std::string& key, int default_val = 0) const;
    double getDouble(const std::string& key, double default_val = 0.0) const;
    bool getBool(const std::string& key, bool default_val = false) const;

    // Setters
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setDouble(const std::string& key, double value);
    void setBool(const std::string& key, bool value);
};

} // namespace sqlopt
