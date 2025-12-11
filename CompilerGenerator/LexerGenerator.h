#pragma once

#include "Types.h"

class LexerGenerator {
public:
    // 初始化
    LexerGenerator();

    // 1. 添加一条词法规则
    // 例如: AddRule("NUM", "[0-9]+")
    void addRule(const std::string& tokenName, const std::string& regex);

    // 2. 核心算法入口：构建 DFA
    // 内部会调用 regexToNFA -> subsetConstruction -> minimize
    void build();

    // 3. 获取生成的 DFA 表 (供成员 C 使用)
    const DFATable& getDFATable() const;

private:
    std::vector<TokenDefinition> rules;
    DFATable dfaTable;

    // 内部辅助函数建议（不需要公开）
    // void regexToNFA(...)
    // void nfaToDFA(...)
};
