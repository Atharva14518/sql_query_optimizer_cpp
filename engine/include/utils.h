#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <sstream>

namespace sqlopt {

inline std::string to_lower(std::string s){
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return std::tolower(c);});
    return s;
}

inline std::string trim(const std::string &s){
    size_t i=0,j=s.size();
    while(i<j && std::isspace((unsigned char)s[i])) ++i;
    while(j>i && std::isspace((unsigned char)s[j-1])) --j;
    return s.substr(i,j-i);
}

inline int levenshtein(const std::string &a, const std::string &b){
    int n=a.size(), m=b.size();
    std::vector<int> prev(m+1), cur(m+1);
    for(int j=0;j<=m;++j) prev[j]=j;
    for(int i=1;i<=n;++i){
        cur[0]=i;
        for(int j=1;j<=m;++j){
            int cost = (a[i-1]==b[j-1]?0:1);
            cur[j]=std::min({prev[j]+1, cur[j-1]+1, prev[j-1]+cost});
        }
        std::swap(prev,cur);
    }
    return prev[m];
}

inline std::string suggest_keyword(const std::string &token, const std::vector<std::string>& keywords){
    int best=1e9; std::string cand;
    for(const auto &k: keywords){
        int d = levenshtein(to_lower(token), to_lower(k));
        if(d<best){ best=d; cand=k; }
    }
    return best<=2 ? cand : std::string();
}

struct TransformEntry{
    std::string stage;
    std::string detail;
    double millis=0.0;
};

struct TransformLog{
    std::vector<TransformEntry> items;
    void add(const std::string &stage, const std::string &detail, double millis=0.0){
        items.push_back({stage,detail,millis});
    }
    std::string str() const{
        std::ostringstream oss;
        for(size_t i=0;i<items.size();++i){
            oss << (i+1) << ". [" << items[i].stage << "] " << items[i].detail << "\n";
        }
        return oss.str();
    }
};

} // namespace sqlopt
