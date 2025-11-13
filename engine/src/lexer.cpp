#include "lexer.h"
#include <cctype>

using namespace sqlopt;

static bool is_ident_start(char c){ return std::isalpha((unsigned char)c) || c=='_'; }
static bool is_ident_char(char c){ return std::isalnum((unsigned char)c) || c=='_' ; }

std::vector<Token> Lexer::tokenize(){
    std::vector<Token> out; int start=0;
    auto push=[&](TokenType t, const std::string &tx){ out.push_back({t,tx,i}); };
    while(i<n){
        char c=s[i];
        if(std::isspace((unsigned char)c)){ ++i; continue; }
        if(c=='*'){ push(TokenType::STAR, "*"); ++i; continue; }
        if(c==','){ push(TokenType::COMMA, ","); ++i; continue; }
        if(c=='.'){ push(TokenType::DOT, "."); ++i; continue; }
        if(c=='('){ push(TokenType::LPAREN, "("); ++i; continue; }
        if(c==')'){ push(TokenType::RPAREN, ")"); ++i; continue; }
        if(c==';'){ push(TokenType::SEMICOLON, ";"); ++i; continue; }
        if(c=='\'' || c=='\"'){
            char q=c; ++i; start=i; std::string val;
            while(i<n && s[i]!=q){ if(s[i]=='\\' && i+1<n) { val.push_back(s[i+1]); i+=2; } else { val.push_back(s[i++]); } }
            if(i<n && s[i]==q) ++i; 
            push(TokenType::STRING, val); continue;
        }
        if(std::isdigit((unsigned char)c)){
            start=i; while(i<n && (std::isdigit((unsigned char)s[i])||s[i]=='.')) ++i;
            push(TokenType::NUMBER, s.substr(start,i-start)); continue;
        }
        if(is_ident_start(c)){
            start=i; while(i<n && is_ident_char(s[i])) ++i;
            std::string id = s.substr(start,i-start);
            std::string low; low.resize(id.size());
            for(size_t k=0;k<id.size();++k) low[k]=std::tolower((unsigned char)id[k]);
            static const std::vector<std::string> kws={"select","from","where","join","on","inner","left","right","full","natural","anti","outer","group","by","order","asc","desc","limit","as","and","having","between","in","sum","count","avg","min","max","or","not","like","any","all","case","insert","update","delete","into","set","values"};
            bool iskw=false; for(auto &kw:kws){ if(low==kw){ iskw=true; break; } }
            push(iskw?TokenType::KW:TokenType::IDENT, id);
            continue;
        }
        if(c=='<'){ if(i+1<n && s[i+1]=='<'){ push(TokenType::OP,"<<"); i+=2; } else { start=i; ++i; if(i<n && s[i]=='=') ++i; push(TokenType::OP, s.substr(start,i-start)); } continue; }
        if(c=='>'){ if(i+1<n && s[i+1]=='>'){ push(TokenType::OP,">>"); i+=2; } else { start=i; ++i; if(i<n && s[i]=='=') ++i; push(TokenType::OP, s.substr(start,i-start)); } continue; }
        if(std::string("=<>!~+-*/%&|^").find(c)!=std::string::npos){
            start=i; ++i; if(i<n && (s[i]=='=' || s[i]=='>' || s[i]=='<' || s[i]=='|')) ++i;
            push(TokenType::OP, s.substr(start,i-start)); continue;
        }
        start=i; ++i; push(TokenType::IDENT, s.substr(start,1));
    }
    out.push_back({TokenType::END,"",i});
    return out;
}
