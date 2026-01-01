#pragma once

#include "Types.h"
#include <set>
#include <map>

// NFA 状态结构
struct NFAState
{
    int id;
    bool isFinal;
    std::string tokenName;                     // 如果是终态，对应的Token名字
    std::map<char, std::set<int>> transitions; // 字符 -> 目标状态集合
    std::set<int> epsilonTransitions;          // epsilon 转换
};

// NFA 结构
struct NFA
{
    int startState;
    int endState;
    std::map<int, NFAState> states;
};

// DFA 状态（用于子集构造）
struct DFASubset
{
    std::set<int> nfaStates; // 包含的NFA状态集合
    bool isFinal;
    std::string tokenName; // 如果有多个终态，选择优先级最高的
    int dfaStateID;
    std::map<char, int> transitions; // 字符 -> 目标DFA状态ID
};

class LexerGenerator
{
public:
    // 初始化
    LexerGenerator();

    // 1. 添加一条词法规则
    // 例如: AddRule("NUM", "[0-9]+")
    void addRule(const std::string &tokenName, const std::string &regex);

    // 2. 核心算法入口：构建 DFA
    // 内部会调用 regexToNFA -> subsetConstruction -> minimize
    void build();

    // 3. 获取生成的 DFA 表 (供成员 C 使用)
    const DFATable &getDFATable() const;

private:
    std::vector<TokenDefinition> rules;
    DFATable dfaTable;

    int nextStateID; // 用于生成唯一的状态ID

    // ========== 核心算法实现 ==========

    // 1. 正则表达式预处理：将字符类展开，转义字符处理
    std::string preprocessRegex(const std::string &regex);

    // 2. 将正则表达式转换为后缀表达式（支持基本操作符）
    std::string regexToPostfix(const std::string &regex);

    // 3. Thompson算法：正则表达式 -> NFA
    NFA regexToNFA(const std::string &regex, const std::string &tokenName);

    // 4. 合并多个NFA（用于处理多条规则）
    NFA mergeNFAs(const std::vector<NFA> &nfas);

    // 5. 计算epsilon闭包
    std::set<int> epsilonClosure(const NFA &nfa, const std::set<int> &states);

    // 6. 计算状态集合在某个字符下的转换
    std::set<int> move(const NFA &nfa, const std::set<int> &states, char c);

    // 7. 子集构造法：NFA -> DFA
    void nfaToDFA(const NFA &nfa);

    // 8. DFA最小化（Hopcroft算法）
    void minimizeDFA();

    // 9. 将内部DFA表示转换为DFATable格式
    void convertToDFATable(const std::map<int, DFASubset> &dfaStates);
};
