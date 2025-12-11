#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

// === 通用定义 ===

// Token 定义 (Scanner 生成时需要，Parser 生成时也需要知道 Token 的名字)
struct TokenDefinition {
    std::string name;    // 例如 "NUM", "ID"
    std::string pattern; // 正则表达式，例如 "[0-9]+"
};

// 产生式规则定义
// 例如: E -> E + T { printf("add"); }
struct ProductionRule {
    int id;                        // 规则编号
    std::string lhs;               // 左部非终结符，例如 "E"
    std::vector<std::string> rhs;  // 右部符号列表，例如 ["E", "+", "T"]
    std::string semanticAction;    // 语义动作代码，例如 "{ $$ = $1 + $3; }"
};

// === 词法分析器产出 ===

// DFA 转换表的一行
struct DFARow {
    int stateID;
    bool isFinal;
    std::string tokenName; //如果是终态，对应的Token名字
    std::map<char, int> transitions; // 输入 char -> 跳转到 stateID
};

using DFATable = std::vector<DFARow>;

// === 语法分析器产出 ===

// LR 分析动作类型
enum ActionType { ACTION_SHIFT, ACTION_REDUCE, ACTION_ACCEPT, ACTION_ERROR };

struct LRAction {
    ActionType type;
    int target; // 如果是 Shift，则是状态 ID；如果是 Reduce，则是规则 ID
};

// LR 分析表
// Key: pair<当前状态ID, 输入符号(Token名或char)>
using ActionTable = std::map<std::pair<int, std::string>, LRAction>;
// Key: pair<当前状态ID, 非终结符> -> 下一个状态ID
using GotoTable = std::map<std::pair<int, std::string>, int>;
