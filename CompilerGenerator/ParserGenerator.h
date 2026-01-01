#pragma once

#include "Types.h"
#include <unordered_map>
#include <unordered_set>

//希腊字母那个空串符号，因为编码格式问题，放进来会报错，先用eps代替
static const std::string EPS = "eps";  // 表示空串的符号，可按需调整
static const std::string END_MARKER = "#";  // 输入结束标记

// LR(1) 项目：[产生式编号, 点位置, 向前看符号]
struct LR1Item {
    int prodId;           // 产生式编号
    size_t dotPos;        // 点的位置（0 表示在最左边）
    std::string lookahead; // 向前看符号

    bool operator==(const LR1Item& other) const {
        return prodId == other.prodId && dotPos == other.dotPos && lookahead == other.lookahead;
    }

    bool operator<(const LR1Item& other) const {
        if (prodId != other.prodId) return prodId < other.prodId;
        if (dotPos != other.dotPos) return dotPos < other.dotPos;
        return this->lookahead < other.lookahead;
    }
};

// LR(1) 项目集
struct LR1ItemSet {
    std::set<LR1Item> items;
    int id;  // 项目集编号

    bool operator==(const LR1ItemSet& other) const {
        return items == other.items;
    }
};

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
	// 增广之后的产生式列表
    std::vector<ProductionRule> augmentedProductions;
    ActionTable actionTable;
    GotoTable gotoTable;

    //区分终结符与非终结符
    void sortSymbols(
        std::unordered_set<std::string>& nonterminals, 
        std::unordered_set<std::string>& terminals, 
        const std::vector<ProductionRule>& productions);

    //获取每个符号的FIRST集合
    std::unordered_map<std::string, std::unordered_set<std::string>> computeFirstSets
        (std::unordered_set<std::string>& nonterminals,
           std::unordered_set<std::string>& terminals, 
            const std::vector<ProductionRule>& productions);
    
    // 计算符号串的 FIRST 集（用于 LR(1) 项目集构造）
    std::unordered_set<std::string> computeFirstOfString(
        const std::vector<std::string>& symbols,
        const std::unordered_map<std::string, std::unordered_set<std::string>>& first);

    // 计算 LR(1) 项目集的闭包
    std::set<LR1Item> closure(
        const std::set<LR1Item>& items,
        const std::vector<ProductionRule>& productions,
        const std::unordered_map<std::string, std::unordered_set<std::string>>& first);

    // 计算 GOTO(I, X)
    std::set<LR1Item> gotoSet(
        const std::set<LR1Item>& items,
        const std::string& symbol,
        const std::vector<ProductionRule>& productions,
        const std::unordered_map<std::string, std::unordered_set<std::string>>& first);

    //构建增广产生式
    std::vector<ProductionRule> buildAugmentedProductions(const std::string& startSymbol, const std::vector<ProductionRule>& productions);

    // 构建 LR(1) 项目集族（注意：productions 参数应已包含增广产生式且位于索引0）
    std::vector<LR1ItemSet> buildLR1ItemSets(
        const std::vector<ProductionRule>& productions,
        const std::unordered_map<std::string, std::unordered_set<std::string>>& first);

    // 构建 LR(1) 分析表
    void buildLR1ParsingTable(
        std::unordered_set<std::string>& nonterminals,
        std::unordered_set<std::string>& terminals,
        const std::vector<LR1ItemSet>& itemSets,
        const std::vector<ProductionRule>& productions,
        ActionTable& actionTable,
        GotoTable& gotoTable);
};
