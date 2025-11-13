#include "planner.h"
#include <sstream>

using namespace sqlopt;

static std::string indent_str(int n){ return std::string(n,' ');
}

std::string ScanNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"Scan(table="<<table;
    if(!alias.empty()) oss<<" AS "<<alias;
    oss<<", rows="<<est_rows<<", cost="<<cost;
    if(!filters.empty()){ oss<<", filters=["; for(size_t i=0;i<filters.size();++i){ if(i) oss<<", "; oss<<filters[i]; } oss<<"]"; }
    oss<<")\n"; return oss.str();
}

std::string IndexScanNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"IndexScan(table="<<table;
    if(!alias.empty()) oss<<" AS "<<alias;
    oss<<", rows="<<est_rows<<", cost="<<cost;
    if(!index_cols.empty()){ oss<<", index=["; for(size_t i=0;i<index_cols.size();++i){ if(i) oss<<","; oss<<index_cols[i]; } oss<<"]"; }
    if(!filters.empty()){ oss<<", filters=["; for(size_t i=0;i<filters.size();++i){ if(i) oss<<", "; oss<<filters[i]; } oss<<"]"; }
    oss<<")\n"; return oss.str();
}

std::string JoinNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<join_type<<" Join(algo="<<algo<<", rows="<<est_rows<<", cost="<<cost;
    if(!conds.empty()){ oss<<", conds=["; for(size_t i=0;i<conds.size();++i){ if(i) oss<<", "; oss<<conds[i]; } oss<<"]"; }
    oss<<")\n";
    oss<<left->explain(indent+2);
    oss<<right->explain(indent+2);
    return oss.str();
}

std::string ProjectNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"Project(rows="<<est_rows<<", cost="<<cost<<", items=[";
    for(size_t i=0;i<items.size();++i){ if(i) oss<<", "; oss<<items[i]; }
    oss<<"])\n"; oss<<child->explain(indent+2); return oss.str();
}

std::string FilterNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"Filter(rows="<<est_rows<<", cost="<<cost<<", conds=[";
    for(size_t i=0;i<conds.size();++i){ if(i) oss<<", "; oss<<conds[i]; }
    oss<<"])\n"; oss<<child->explain(indent+2); return oss.str();
}

std::string SortNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"Sort(rows="<<est_rows<<", cost="<<cost<<")\n";
    oss<<child->explain(indent+2); return oss.str();
}

std::string LimitNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"Limit(n="<<limit<<", rows="<<est_rows<<", cost="<<cost<<")\n";
    oss<<child->explain(indent+2); return oss.str();
}

std::string DistinctNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"Distinct(rows="<<est_rows<<", cost="<<cost<<")\n";
    oss<<child->explain(indent+2); return oss.str();
}

std::string HavingNode::explain(int indent) const{
    std::ostringstream oss; oss<<indent_str(indent)<<"Having(rows="<<est_rows<<", cost="<<cost<<", conds=[";
    for(size_t i=0;i<conds.size();++i){ if(i) oss<<", "; oss<<conds[i]; }
    oss<<"])\n"; oss<<child->explain(indent+2); return oss.str();
}
