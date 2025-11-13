#pragma once
#include <string>
#include "ast.h"
#include "statistics_manager.h"

namespace sqlopt {

bool semantic_validate(const SelectQuery &q, const StatisticsManager &stats, std::string &err_out);

} // namespace sqlopt
