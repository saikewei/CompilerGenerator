#include "LexerGenerator.h"
#include <iostream>
#include <stack>
#include <queue>
#include <algorithm>
#include <sstream>
#include <cctype>

// 构造函数
LexerGenerator::LexerGenerator() : nextStateID(0) {
}

void LexerGenerator::addRule(const std::string& tokenName, const std::string& regex) {
    TokenDefinition def = { tokenName, regex };
    this->rules.push_back(def);
    std::cout << "[LexerGen] Added rule: " << tokenName << " -> " << regex << std::endl;
}

// ========== 正则表达式预处理 ==========
std::string LexerGenerator::preprocessRegex(const std::string& regex) {
    std::string result;
    bool inCharClass = false;
    bool escape = false;
    
    for (size_t i = 0; i < regex.length(); ++i) {
        char c = regex[i];
        
        if (escape) {
            // 处理转义字符
            if (c == 'n') result += '\n';
            else if (c == 't') result += '\t';
            else if (c == 'r') result += '\r';
            else result += c;
            escape = false;
        }
        else if (c == '\\') {
            escape = true;
        }
        else if (c == '[' && !inCharClass) {
            // 开始字符类
            inCharClass = true;
            result += '(';
        }
        else if (c == ']' && inCharClass) {
            // 结束字符类，转换为选择表达式
            inCharClass = false;
            // 字符类会在后续处理中展开
            result += ')';
        }
        else if (inCharClass) {
            // 在字符类内部
            if (c == '-' && i > 0 && i + 1 < regex.length()) {
                // 处理范围，如 [0-9] -> (0|1|2|3|4|5|6|7|8|9)
                char start = regex[i - 1];
                char end = regex[i + 1];
                result.pop_back(); // 移除刚添加的起始字符
                result += '(';
                for (char ch = start; ch <= end; ++ch) {
                    result += ch;
                    if (ch < end) result += '|';
                }
                result += ')';
                i++; // 跳过结束字符
            }
            else {
                result += c;
            }
        }
        else {
            result += c;
        }
    }
    
    return result;
}

// ========== 正则表达式转后缀表达式 ==========
std::string LexerGenerator::regexToPostfix(const std::string& regex) {
    std::string postfix;
    std::stack<char> opStack;
    
    // 运算符优先级
    auto precedence = [](char op) -> int {
        if (op == '*') return 3;
        if (op == '+') return 3;
        if (op == '?') return 3;
        if (op == '|') return 1;
        if (op == '.') return 2;  // 连接运算符
        return 0;
    };
    
    // 添加显式的连接运算符
    std::string withConcat;
    for (size_t i = 0; i < regex.length(); ++i) {
        char c = regex[i];
        withConcat += c;
        
        if (i + 1 < regex.length()) {
            char next = regex[i + 1];
            // 需要添加连接运算符的情况
            if ((c != '(' && c != '|') && (next != ')' && next != '*' && next != '+' && next != '?' && next != '|')) {
                withConcat += '.';  // 隐式连接运算符
            }
        }
    }
    
    // 转换为后缀表达式
    for (char c : withConcat) {
        if (isalnum(c) || c == '\\' || c == '\n' || c == '\t' || c == '\r') {
            postfix += c;
        }
        else if (c == '(') {
            opStack.push(c);
        }
        else if (c == ')') {
            while (!opStack.empty() && opStack.top() != '(') {
                postfix += opStack.top();
                opStack.pop();
            }
            if (!opStack.empty()) opStack.pop();
        }
        else {
            // 运算符
            while (!opStack.empty() && opStack.top() != '(' &&
                   precedence(opStack.top()) >= precedence(c)) {
                postfix += opStack.top();
                opStack.pop();
            }
            opStack.push(c);
        }
    }
    
    while (!opStack.empty()) {
        postfix += opStack.top();
        opStack.pop();
    }
    
    return postfix;
}

// ========== Thompson算法：正则表达式 -> NFA ==========
NFA LexerGenerator::regexToNFA(const std::string& regex, const std::string& tokenName) {
    std::string processed = preprocessRegex(regex);
    std::string postfix = regexToPostfix(processed);
    
    std::stack<NFA> nfaStack;
    
    for (char c : postfix) {
        if (c == '.') {
            // 连接操作
            if (nfaStack.size() < 2) continue;
            NFA nfa2 = nfaStack.top(); nfaStack.pop();
            NFA nfa1 = nfaStack.top(); nfaStack.pop();
            
            // 连接 nfa1 和 nfa2
            int newStart = nextStateID++;
            int newEnd = nextStateID++;
            
            NFAState startState;
            startState.id = newStart;
            startState.epsilonTransitions.insert(nfa1.startState);
            
            NFAState endState;
            endState.id = newEnd;
            endState.isFinal = nfa2.states[nfa2.endState].isFinal;
            endState.tokenName = nfa2.states[nfa2.endState].tokenName;
            
            // 将 nfa1 的终态连接到 nfa2 的起始状态
            nfa1.states[nfa1.endState].epsilonTransitions.insert(nfa2.startState);
            nfa1.states[nfa1.endState].isFinal = false;
            
            NFA result;
            result.startState = newStart;
            result.endState = newEnd;
            result.states = nfa1.states;
            result.states.insert(nfa2.states.begin(), nfa2.states.end());
            result.states[newStart] = startState;
            result.states[newEnd] = endState;
            
            nfaStack.push(result);
        }
        else if (c == '|') {
            // 选择操作
            if (nfaStack.size() < 2) continue;
            NFA nfa2 = nfaStack.top(); nfaStack.pop();
            NFA nfa1 = nfaStack.top(); nfaStack.pop();
            
            int newStart = nextStateID++;
            int newEnd = nextStateID++;
            
            NFAState startState;
            startState.id = newStart;
            startState.epsilonTransitions.insert(nfa1.startState);
            startState.epsilonTransitions.insert(nfa2.startState);
            
            NFAState endState;
            endState.id = newEnd;
            endState.isFinal = true;
            endState.tokenName = tokenName;
            
            // 将两个NFA的终态连接到新的终态
            nfa1.states[nfa1.endState].epsilonTransitions.insert(newEnd);
            nfa1.states[nfa1.endState].isFinal = false;
            nfa2.states[nfa2.endState].epsilonTransitions.insert(newEnd);
            nfa2.states[nfa2.endState].isFinal = false;
            
            NFA result;
            result.startState = newStart;
            result.endState = newEnd;
            result.states = nfa1.states;
            result.states.insert(nfa2.states.begin(), nfa2.states.end());
            result.states[newStart] = startState;
            result.states[newEnd] = endState;
            
            nfaStack.push(result);
        }
        else if (c == '*') {
            // Kleene闭包
            if (nfaStack.empty()) continue;
            NFA nfa = nfaStack.top(); nfaStack.pop();
            
            int newStart = nextStateID++;
            int newEnd = nextStateID++;
            
            NFAState startState;
            startState.id = newStart;
            startState.epsilonTransitions.insert(nfa.startState);
            startState.epsilonTransitions.insert(newEnd);  // 允许0次
            
            NFAState endState;
            endState.id = newEnd;
            endState.isFinal = true;
            endState.tokenName = tokenName;
            
            // 将原NFA的终态连接到起始状态（循环）和新终态
            nfa.states[nfa.endState].epsilonTransitions.insert(nfa.startState);
            nfa.states[nfa.endState].epsilonTransitions.insert(newEnd);
            nfa.states[nfa.endState].isFinal = false;
            
            NFA result;
            result.startState = newStart;
            result.endState = newEnd;
            result.states = nfa.states;
            result.states[newStart] = startState;
            result.states[newEnd] = endState;
            
            nfaStack.push(result);
        }
        else if (c == '+') {
            // 正闭包（至少一次）
            if (nfaStack.empty()) continue;
            NFA nfa = nfaStack.top(); nfaStack.pop();
            
            int newStart = nextStateID++;
            int newEnd = nextStateID++;
            
            NFAState startState;
            startState.id = newStart;
            startState.epsilonTransitions.insert(nfa.startState);
            
            NFAState endState;
            endState.id = newEnd;
            endState.isFinal = true;
            endState.tokenName = tokenName;
            
            // 将原NFA的终态连接到起始状态（循环）和新终态
            nfa.states[nfa.endState].epsilonTransitions.insert(nfa.startState);
            nfa.states[nfa.endState].epsilonTransitions.insert(newEnd);
            nfa.states[nfa.endState].isFinal = false;
            
            NFA result;
            result.startState = newStart;
            result.endState = newEnd;
            result.states = nfa.states;
            result.states[newStart] = startState;
            result.states[newEnd] = endState;
            
            nfaStack.push(result);
        }
        else if (c == '?') {
            // 可选（0次或1次）
            if (nfaStack.empty()) continue;
            NFA nfa = nfaStack.top(); nfaStack.pop();
            
            int newStart = nextStateID++;
            int newEnd = nextStateID++;
            
            NFAState startState;
            startState.id = newStart;
            startState.epsilonTransitions.insert(nfa.startState);
            startState.epsilonTransitions.insert(newEnd);  // 允许0次
            
            NFAState endState;
            endState.id = newEnd;
            endState.isFinal = true;
            endState.tokenName = tokenName;
            
            nfa.states[nfa.endState].epsilonTransitions.insert(newEnd);
            nfa.states[nfa.endState].isFinal = false;
            
            NFA result;
            result.startState = newStart;
            result.endState = newEnd;
            result.states = nfa.states;
            result.states[newStart] = startState;
            result.states[newEnd] = endState;
            
            nfaStack.push(result);
        }
        else {
            // 单个字符
            int start = nextStateID++;
            int end = nextStateID++;
            
            NFAState startState;
            startState.id = start;
            startState.transitions[c].insert(end);
            
            NFAState endState;
            endState.id = end;
            endState.isFinal = true;
            endState.tokenName = tokenName;
            
            NFA nfa;
            nfa.startState = start;
            nfa.endState = end;
            nfa.states[start] = startState;
            nfa.states[end] = endState;
            
            nfaStack.push(nfa);
        }
    }
    
    if (nfaStack.empty()) {
        // 返回空NFA
        NFA emptyNFA;
        emptyNFA.startState = nextStateID++;
        emptyNFA.endState = nextStateID++;
        return emptyNFA;
    }
    
    return nfaStack.top();
}

// ========== 合并多个NFA ==========
NFA LexerGenerator::mergeNFAs(const std::vector<NFA>& nfas) {
    if (nfas.empty()) {
        NFA emptyNFA;
        emptyNFA.startState = nextStateID++;
        emptyNFA.endState = nextStateID++;
        return emptyNFA;
    }
    
    int newStart = nextStateID++;
    
    NFAState startState;
    startState.id = newStart;
    
    NFA merged;
    merged.startState = newStart;
    
    // 合并所有NFA
    for (const auto& nfa : nfas) {
        startState.epsilonTransitions.insert(nfa.startState);
        merged.states.insert(nfa.states.begin(), nfa.states.end());
    }
    
    merged.states[newStart] = startState;
    
    return merged;
}

// ========== 计算epsilon闭包 ==========
std::set<int> LexerGenerator::epsilonClosure(const NFA& nfa, const std::set<int>& states) {
    std::set<int> closure = states;
    std::queue<int> workQueue;
    
    for (int state : states) {
        workQueue.push(state);
    }
    
    while (!workQueue.empty()) {
        int current = workQueue.front();
        workQueue.pop();
        
        if (nfa.states.find(current) != nfa.states.end()) {
            const NFAState& state = nfa.states.at(current);
            for (int next : state.epsilonTransitions) {
                if (closure.find(next) == closure.end()) {
                    closure.insert(next);
                    workQueue.push(next);
                }
            }
        }
    }
    
    return closure;
}

// ========== 计算状态集合在某个字符下的转换 ==========
std::set<int> LexerGenerator::move(const NFA& nfa, const std::set<int>& states, char c) {
    std::set<int> result;
    
    for (int state : states) {
        if (nfa.states.find(state) != nfa.states.end()) {
            const NFAState& nfaState = nfa.states.at(state);
            if (nfaState.transitions.find(c) != nfaState.transitions.end()) {
                const std::set<int>& targets = nfaState.transitions.at(c);
                result.insert(targets.begin(), targets.end());
            }
        }
    }
    
    return result;
}

// ========== 子集构造法：NFA -> DFA ==========
void LexerGenerator::nfaToDFA(const NFA& nfa) {
    std::map<std::set<int>, int> stateMap;  // NFA状态集合 -> DFA状态ID
    std::map<int, DFASubset> dfaStates;     // DFA状态ID -> DFA状态信息
    std::queue<std::set<int>> workQueue;
    int dfaStateCounter = 0;
    
    // 初始化：计算起始状态的epsilon闭包
    std::set<int> startClosure = epsilonClosure(nfa, {nfa.startState});
    stateMap[startClosure] = dfaStateCounter;
    
    DFASubset startSubset;
    startSubset.nfaStates = startClosure;
    startSubset.dfaStateID = dfaStateCounter;
    startSubset.isFinal = false;
    
    // 检查是否包含终态
    for (int state : startClosure) {
        if (nfa.states.find(state) != nfa.states.end() && nfa.states.at(state).isFinal) {
            startSubset.isFinal = true;
            // 如果有多个终态，选择第一个遇到的（可以根据优先级调整）
            if (startSubset.tokenName.empty()) {
                startSubset.tokenName = nfa.states.at(state).tokenName;
            }
        }
    }
    
    dfaStates[dfaStateCounter++] = startSubset;
    workQueue.push(startClosure);
    
    // 收集所有可能的输入字符
    std::set<char> alphabet;
    for (const auto& pair : nfa.states) {
        for (const auto& trans : pair.second.transitions) {
            alphabet.insert(trans.first);
        }
    }
    
    // 子集构造主循环
    while (!workQueue.empty()) {
        std::set<int> currentSet = workQueue.front();
        workQueue.pop();
        int currentDFAState = stateMap[currentSet];
        
        // 对每个输入字符
        for (char c : alphabet) {
            std::set<int> nextSet = move(nfa, currentSet, c);
            if (nextSet.empty()) continue;
            
            nextSet = epsilonClosure(nfa, nextSet);
            
            int nextDFAState;
            if (stateMap.find(nextSet) == stateMap.end()) {
                // 新状态
                nextDFAState = dfaStateCounter;
                stateMap[nextSet] = nextDFAState;
                
                DFASubset newSubset;
                newSubset.nfaStates = nextSet;
                newSubset.dfaStateID = nextDFAState;
                newSubset.isFinal = false;
                
                // 检查是否包含终态
                for (int state : nextSet) {
                    if (nfa.states.find(state) != nfa.states.end() && nfa.states.at(state).isFinal) {
                        newSubset.isFinal = true;
                        if (newSubset.tokenName.empty()) {
                            newSubset.tokenName = nfa.states.at(state).tokenName;
                        }
                    }
                }
                
                dfaStates[nextDFAState] = newSubset;
                workQueue.push(nextSet);
                dfaStateCounter++;
            }
            else {
                nextDFAState = stateMap[nextSet];
            }
            
            // 记录转换
            dfaStates[currentDFAState].transitions[c] = nextDFAState;
        }
    }
    
    // 转换为DFATable格式
    convertToDFATable(dfaStates);
}

// ========== DFA最小化（Hopcroft算法）==========
void LexerGenerator::minimizeDFA() {
    if (dfaTable.empty()) return;
    
    // 将DFATable转换为内部表示以便处理
    std::map<int, DFASubset> dfaStates;
    std::map<int, std::map<char, int>> transitions;
    
    for (const auto& row : dfaTable) {
        DFASubset subset;
        subset.dfaStateID = row.stateID;
        subset.isFinal = row.isFinal;
        subset.tokenName = row.tokenName;
        dfaStates[row.stateID] = subset;
        
        for (const auto& trans : row.transitions) {
            transitions[row.stateID][trans.first] = trans.second;
        }
    }
    
    // Hopcroft算法：基于等价类划分
    std::set<int> acceptingStates;
    std::set<int> nonAcceptingStates;
    
    for (const auto& pair : dfaStates) {
        if (pair.second.isFinal) {
            acceptingStates.insert(pair.first);
        }
        else {
            nonAcceptingStates.insert(pair.first);
        }
    }
    
    std::vector<std::set<int>> partitions;
    if (!acceptingStates.empty()) partitions.push_back(acceptingStates);
    if (!nonAcceptingStates.empty()) partitions.push_back(nonAcceptingStates);
    
    if (partitions.empty()) return;
    
    // 迭代细化划分
    bool changed = true;
    while (changed) {
        changed = false;
        std::vector<std::set<int>> newPartitions;
        
        for (const auto& partition : partitions) {
            if (partition.size() <= 1) {
                newPartitions.push_back(partition);
                continue;
            }
            
            // 收集所有可能的输入字符
            std::set<char> alphabet;
            for (int state : partition) {
                if (transitions.find(state) != transitions.end()) {
                    for (const auto& trans : transitions.at(state)) {
                        alphabet.insert(trans.first);
                    }
                }
            }
            
            // 根据转换目标划分
            std::map<std::map<char, int>, std::set<int>> groups;
            for (int state : partition) {
                std::map<char, int> transMap;
                if (transitions.find(state) != transitions.end()) {
                    for (char c : alphabet) {
                        if (transitions.at(state).find(c) != transitions.at(state).end()) {
                            int target = transitions.at(state).at(c);
                            // 找到目标所在的划分
                            int targetPartition = -1;
                            for (size_t i = 0; i < partitions.size(); ++i) {
                                if (partitions[i].find(target) != partitions[i].end()) {
                                    targetPartition = i;
                                    break;
                                }
                            }
                            transMap[c] = targetPartition;
                        }
                    }
                }
                groups[transMap].insert(state);
            }
            
            if (groups.size() > 1) {
                changed = true;
                for (const auto& group : groups) {
                    newPartitions.push_back(group.second);
                }
            }
            else {
                newPartitions.push_back(partition);
            }
        }
        
        partitions = newPartitions;
    }
    
    // 构建最小化后的DFA
    std::map<int, int> stateMapping;  // 旧状态 -> 新状态
    std::map<int, DFASubset> minimizedStates;
    int newStateID = 0;
    
    for (const auto& partition : partitions) {
        int representative = *partition.begin();
        stateMapping[representative] = newStateID;
        
        DFASubset newSubset;
        newSubset.dfaStateID = newStateID;
        newSubset.isFinal = dfaStates[representative].isFinal;
        newSubset.tokenName = dfaStates[representative].tokenName;
        
        // 构建转换
        if (transitions.find(representative) != transitions.end()) {
            for (const auto& trans : transitions.at(representative)) {
                int oldTarget = trans.second;
                // 找到目标所在的划分
                int targetPartition = -1;
                for (size_t i = 0; i < partitions.size(); ++i) {
                    if (partitions[i].find(oldTarget) != partitions[i].end()) {
                        targetPartition = i;
                        break;
                    }
                }
                if (targetPartition >= 0) {
                    int newTarget = stateMapping[*partitions[targetPartition].begin()];
                    newSubset.transitions[trans.first] = newTarget;
                }
            }
        }
        
        minimizedStates[newStateID] = newSubset;
        newStateID++;
    }
    
    // 更新DFATable
    convertToDFATable(minimizedStates);
}

// ========== 转换为DFATable格式 ==========
void LexerGenerator::convertToDFATable(const std::map<int, DFASubset>& dfaStates) {
    dfaTable.clear();
    
    for (const auto& pair : dfaStates) {
        DFARow row;
        row.stateID = pair.second.dfaStateID;
        row.isFinal = pair.second.isFinal;
        row.tokenName = pair.second.tokenName;
        
        // 转换transitions
        for (const auto& trans : pair.second.transitions) {
            row.transitions[trans.first] = trans.second;
        }
        
        dfaTable.push_back(row);
    }
}

// ========== 主构建函数 ==========
void LexerGenerator::build() {
    std::cout << "[LexerGen] Building DFA from " << rules.size() << " rules..." << std::endl;
    
    if (rules.empty()) {
        std::cout << "[LexerGen] Warning: No rules to process." << std::endl;
        return;
    }
    
    // 1. 为每条规则构建NFA
    std::vector<NFA> nfas;
    for (const auto& rule : rules) {
        std::cout << "[LexerGen] Converting regex to NFA: " << rule.name << " -> " << rule.pattern << std::endl;
        NFA nfa = regexToNFA(rule.pattern, rule.name);
        nfas.push_back(nfa);
    }
    
    // 2. 合并所有NFA
    std::cout << "[LexerGen] Merging NFAs..." << std::endl;
    NFA mergedNFA = mergeNFAs(nfas);
    
    // 3. NFA -> DFA (子集构造法)
    std::cout << "[LexerGen] Converting NFA to DFA (Subset Construction)..." << std::endl;
    nfaToDFA(mergedNFA);
    
    std::cout << "[LexerGen] DFA has " << dfaTable.size() << " states." << std::endl;
    
    // 4. DFA最小化（可选）
    std::cout << "[LexerGen] Minimizing DFA (Hopcroft Algorithm)..." << std::endl;
    minimizeDFA();
    
    std::cout << "[LexerGen] Minimized DFA has " << dfaTable.size() << " states." << std::endl;
    std::cout << "[LexerGen] Build complete!" << std::endl;
}

const DFATable& LexerGenerator::getDFATable() const {
    return this->dfaTable;
}
