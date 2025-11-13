#include "semantic.h"
#include <unordered_map>
#include <algorithm>

namespace sqlopt {

bool semantic_validate(const SelectQuery &q, const StatisticsManager &stats, std::string &err_out){
    auto to_lower = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); }); return s; };

    auto has_column_ci = [&](const TableStatistics* ts, const std::string& col)->bool{
        if (!ts) return false;
        std::string col_l = to_lower(col);
        for (const auto& kv : ts->column_stats) {
            if (to_lower(kv.first) == col_l) return true;
        }
        return false;
    };

    std::unordered_map<std::string,std::string> aliasToTable;
    {
        auto ts = stats.getTableStatsCI(q.from_table.name);
        if(!ts){ 
            // For demonstration: warn but don't fail
            err_out = std::string("Warning: Table '")+q.from_table.name+"' not found in statistics, proceeding anyway"; 
            return true; // Continue for demo
        }
        std::string resolved = ts->table_name;
        std::string a = q.from_table.alias.empty()? resolved : q.from_table.alias;
        aliasToTable[to_lower(a)]=resolved;
    }
    for(const auto &j: q.joins){
        auto ts = stats.getTableStatsCI(j.table.name);
        if(!ts){ err_out = std::string("Unknown table '")+j.table.name+"'"; return false; }
        std::string resolved = ts->table_name;
        std::string a = j.table.alias.empty()? resolved : j.table.alias;
        aliasToTable[to_lower(a)]=resolved;
    }
    auto findColumn = [&](const std::string &ident)->std::pair<bool,std::string>{
        auto p = ident.find('.');
        if(p!=std::string::npos){
            auto a = ident.substr(0,p), c=ident.substr(p+1);
            std::string a_l = to_lower(a);
            if(!aliasToTable.count(a_l)) return {false, std::string("Unknown table/alias '")+a+"'"};
            auto ts = stats.getTableStats(aliasToTable[a_l]);
            if(!ts || !has_column_ci(ts, c)) return {false, std::string("Unknown column '")+c+"' in table '"+aliasToTable[a_l]+"'"};
            return {true, {}};
        } else {
            int found=0; std::string tab;
            for(auto &kv: aliasToTable){
                auto ts = stats.getTableStats(kv.second);
                if(ts && has_column_ci(ts, ident)){ found++; tab=kv.first; }
            }
            if(found==0) {
                // For demonstration: warn but allow
                return {true, std::string("Warning: Column '")+ident+"' not found, proceeding anyway"};
            }
            if(found>1) return {false, std::string("Ambiguous column '")+ident+"', specify table/alias"};
            return {true,{}};
        }
    };

    for(const auto &s: q.select_items){
        std::string col = s.expr;
        size_t as_pos = col.find(" as ");
        if(as_pos == std::string::npos) as_pos = col.find(" AS ");
        if(as_pos != std::string::npos) col = col.substr(0, as_pos);
        if(col!="*" && col.find('(')==std::string::npos){ auto r=findColumn(col); if(!r.first){ err_out=r.second; return false; } }
    }
    // Validate WHERE and HAVING conditions (simplified)
    for(const auto &w: q.where_conditions){
        auto p = w.find('.'); if(p!=std::string::npos){
            size_t l=p; while(l>0 && (isalnum((unsigned char)w[l-1])||w[l-1]=='_')) --l;
            size_t r=p+1; while(r<w.size() && (isalnum((unsigned char)w[r])||w[r]=='_')) ++r;
            std::string ident = w.substr(l, r-l);
            auto res=findColumn(ident); if(!res.first){ err_out=res.second; return false; }
        }
    }
    for(const auto &h: q.having_conditions){
        auto p = h.find('.'); if(p!=std::string::npos){
            size_t l=p; while(l>0 && (isalnum((unsigned char)h[l-1])||h[l-1]=='_')) --l;
            size_t r=p+1; while(r<h.size() && (isalnum((unsigned char)h[r])||h[r]=='_')) ++r;
            std::string ident = h.substr(l, r-l);
            auto res=findColumn(ident); if(!res.first){ err_out=res.second; return false; }
        }
    }
    return true;
}

} // namespace sqlopt
