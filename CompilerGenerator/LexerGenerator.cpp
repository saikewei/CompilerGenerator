#include "LexerGenerator.h"
#include <iostream>
#include <stack>
#include <queue>
#include <algorithm>
#include <sstream>
#include <cctype>
#include <stdexcept>

static const char LITERAL_MARK = '\x01';

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
    const size_t n = regex.size();

    auto decodeEscape = [](char c) -> char {
        switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        default:  return c;   // 普通转义，如 \+ \* 等
        }
        };

    for (size_t i = 0; i < n; ++i) {
        char c = regex[i];

        /* ========== 字符类处理 ========= */
        if (c == '[') {
            result.push_back('(');
            bool first = true;

            ++i; // 跳过 '['
            while (i < n && regex[i] != ']') {
                if (!first) {
                    result.push_back('|');
                }
                first = false;

                char startChar;

                /* ---- 处理转义 ---- */
                if (regex[i] == '\\') {
                    if (i + 1 >= n)
                        throw std::runtime_error("Invalid escape in character class");

                    startChar = decodeEscape(regex[i + 1]);
                    i += 2;
                }
                else {
                    startChar = regex[i];
                    ++i;
                }

                /* ---- 检查是否是范围 a-z ---- */
                if (i + 1 < n && regex[i] == '-' && regex[i + 1] != ']') {
                    i++; // 跳过 '-'

                    char endChar;
                    if (regex[i] == '\\') {
                        if (i + 1 >= n)
                            throw std::runtime_error("Invalid escape in range");
                        endChar = decodeEscape(regex[i + 1]);
                        i += 2;
                    }
                    else {
                        endChar = regex[i];
                        ++i;
                    }

                    if (startChar > endChar)
                        throw std::runtime_error("Invalid range");

                    for (char ch = startChar; ch <= endChar; ++ch) {
                        if (ch != startChar)
                            result.push_back('|');
                        result.push_back(ch);
                    }
                }
                else {
                    result.push_back(startChar);
                }
            }

            if (i >= n || regex[i] != ']')
                throw std::runtime_error("Unclosed character class");

            result.push_back(')');
        }

        /* ========== 普通转义处理 ========= */
        else if (c == '\\') {
            if (i + 1 >= n)
                throw std::runtime_error("Dangling escape");

            char decoded = decodeEscape(regex[i + 1]);

            // 控制字符直接写入
            if (regex[i + 1] == 'n' || regex[i + 1] == 't' || regex[i + 1] == 'r') {
                result.push_back(decoded);
            }
            // 元字符转义，保留反斜杠
            else {
                result.push_back('\\');
                result.push_back(decoded);
            }
            ++i;
        }

        /* ========== 普通字符 ========= */
        else {
            result.push_back(c);
        }
    }

    return result;
}

// ========== 正则表达式转后缀表达式 ==========
std::string LexerGenerator::regexToPostfix(const std::string& regex) {
    std::string postfix;
    std::stack<char> opStack;

    auto precedence = [](char op) -> int {
        if (op == '*') return 3;
        if (op == '+') return 3;
        if (op == '?') return 3;
        if (op == '|') return 1;
        if (op == '.') return 2;
        return 0;
        };

    // 1. 添加显式连接符 '.'
    std::string withConcat;
    for (size_t i = 0; i < regex.length(); ++i) {
        char c = regex[i];

        // 处理转义：如果遇到斜杠，直接把斜杠和下一个字符都加进去，不加连接符逻辑
        if (c == '\\') {
            withConcat += c;
            if (i + 1 < regex.length()) {
                withConcat += regex[i + 1];
                i++; // 跳过下一个字符
            }
            // 判断是否需要追加连接符 (转义序列视为一个整体操作数)
            if (i + 1 < regex.length()) {
                char next = regex[i + 1];
                if (next != ')' && next != '*' && next != '+' && next != '?' && next != '|' && next != '.') {
                    withConcat += '.';
                }
            }
            continue;
        }

        withConcat += c;
        if (i + 1 < regex.length()) {
            char next = regex[i + 1];
            bool isOp = (c == '(' || c == '|' || c == '.');
            // 注意：现在 c 可能是字符类展开后的 '(' 或 ')'，要小心判断

            bool nextIsOp = (next == ')' || next == '*' || next == '+' || next == '?' || next == '|' || next == '.');

            if (!isOp && !nextIsOp) {
                withConcat += '.';
            }
        }
    }

    // 2. 转换为后缀
    for (size_t i = 0; i < withConcat.length(); ++i) {
        char c = withConcat[i];

        if (c == '\\') {
            if (i + 1 < withConcat.length()) {
                postfix += LITERAL_MARK;          // 标记：这是字面字符
                postfix += withConcat[i + 1];     // 实际字符，如 + *
                i++;
            }
            continue;
        }
        // 修改：允许下划线 _ 和其他符号作为操作数
        else if (isalnum(c) || c == '_' || c == '=' || c == '<' || c == '>' || c == ';' || c == '{' || c == '}' || c == ' ' || c == '\n' || c == '\t' || c == '\r') {
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
    
    for (size_t i = 0; i < postfix.size(); ++i) {
        char c = postfix[i];
        if (c == '\x01') {   // LITERAL_MARK
            char literal = postfix[++i];

            int start = nextStateID++;
            int end = nextStateID++;

            NFAState startState;
            startState.id = start;
            startState.transitions[literal].insert(end);

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
            continue;
        } else if (c == '.') {
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
    std::set<int> nonAcceptingStates;
    std::map<std::string, std::set<int>> acceptingGroups; // 按 Token 名分类

    for (const auto& pair : dfaStates) {
        if (pair.second.isFinal) {
            // 根据 TokenName 分组
            acceptingGroups[pair.second.tokenName].insert(pair.first);
        }
        else {
            nonAcceptingStates.insert(pair.first);
        }
    }

    std::vector<std::set<int>> partitions;
    if (!nonAcceptingStates.empty()) {
        partitions.push_back(nonAcceptingStates);
    }

    // 将每种 Token 类型的终态作为独立的初始划分
    for (const auto& group : acceptingGroups) {
        partitions.push_back(group.second);
    }

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
    // 找出原 Start State (0) 在哪个 partition
    int startPartitionIndex = -1;
    for (size_t i = 0; i < partitions.size(); ++i) {
        if (partitions[i].count(0)) {
            startPartitionIndex = i;
            break;
        }
    }

    std::map<int, int> stateMapping; // 旧状态 ID -> 新状态 ID
    std::map<int, DFASubset> minimizedStates;

    int currentID = 1;

    // 第一步：分配 ID
    for (size_t i = 0; i < partitions.size(); ++i) {
        int newID;
        if (i == startPartitionIndex) {
            newID = 0; // 强制归位
        }
        else {
            newID = currentID++;
        }

        // 记录所有旧状态的映射
        for (int oldState : partitions[i]) {
            stateMapping[oldState] = newID;
        }

        // 初始化新状态
        DFASubset newSubset;
        newSubset.dfaStateID = newID;

        // 属性合并
        for (int oldState : partitions[i]) {
            if (dfaStates[oldState].isFinal) {
                newSubset.isFinal = true;
                // 如果有多个 Token 名，保留第一个非空的
                if (newSubset.tokenName.empty()) {
                    newSubset.tokenName = dfaStates[oldState].tokenName;
                }
            }
        }
        minimizedStates[newID] = newSubset;
    }

    // 第二步：构建转换边
    for (size_t i = 0; i < partitions.size(); ++i) {
        int representative = *partitions[i].begin(); // 随便取一个代表
        int srcNewID = stateMapping[representative];

        if (transitions.find(representative) != transitions.end()) {
            for (const auto& trans : transitions.at(representative)) {
                char inputChar = trans.first;
                int oldTarget = trans.second;

                // 查找目标状态的新 ID
                if (stateMapping.find(oldTarget) != stateMapping.end()) {
                    int dstNewID = stateMapping[oldTarget];
                    minimizedStates[srcNewID].transitions[inputChar] = dstNewID;
                }
            }
        }
    }

    // 更新成员变量
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
