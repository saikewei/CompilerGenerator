#pragma once

#include "Types.h"

class ParserGenerator {
public:
    ParserGenerator();

    // 1. 设置起始符号
    void setStartSymbol(const std::string& startSymbol);

    // 2. 添加一条语法规则
    // lhs: "E", rhs: {"E", "+", "T"}, action: "{ ... }"
    void addProduction(const std::string& lhs, const std::vector<std::string>& rhs, const std::string& actionCode);

    // 3. 核心算法入口：构建 LR 分析表
    // 内部调用 computeFirst, computeFollow, buildItems
    void build();

    // 4. 获取结果 (供成员 C 使用)
    const ActionTable& getActionTable() const;
    const GotoTable& getGotoTable() const;
    const std::vector<ProductionRule>& getRules() const;

private:
    std::string startSymbol;
    std::vector<ProductionRule> productions;
    ActionTable actionTable;
    GotoTable gotoTable;

    // 内部辅助函数建议
    // void computeFirstSets();
    // void buildCanonicalCollection();
};
