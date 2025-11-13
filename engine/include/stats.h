#pragma once
#include <string>
#include <map>
#include <vector>
#include <set>

namespace sqlopt {

struct TableStats{
    std::string name;
    double row_count = 100000;
    std::set<std::string> columns;
    std::map<std::string,int> distinct_vals;
    std::vector<std::vector<std::string>> indexes;
};

struct StatsCatalog{
    std::map<std::string, TableStats> tables;
    void load_defaults();
};

} // namespace sqlopt
