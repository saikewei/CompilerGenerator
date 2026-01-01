#include "LexerGenerator.h"
#include <stack>
#include <queue>
#include <algorithm>
#include <cctype>

LexerGenerator::LexerGenerator() : nextStateID(0) {}

void LexerGenerator::addRule(const std::string &tokenName, const std::string &regex)
{
    TokenDefinition def;
    def.name = tokenName;
    def.pattern = regex;
    rules.push_back(def);
}

void LexerGenerator::build()
{
    if (rules.empty())
        return;

    std::vector<NFA> nfas;

    // 为每条规则构建 NFA
    for (const auto &rule : rules)
    {
        NFA nfa = regexToNFA(rule.pattern, rule.name);
        nfas.push_back(nfa);
    }

    // 合并所有 NFA
    NFA mergedNFA = mergeNFAs(nfas);

    // NFA -> DFA (子集构造法)
    nfaToDFA(mergedNFA);

    // DFA 最小化
    minimizeDFA();
}

const DFATable &LexerGenerator::getDFATable() const
{
    return dfaTable;
}

// 预处理正则表达式：展开字符类，处理转义
std::string LexerGenerator::preprocessRegex(const std::string &regex)
{
    std::string result;
    size_t i = 0;

    while (i < regex.size())
    {
        if (regex[i] == '\\' && i + 1 < regex.size())
        {
            // 转义字符
            char next = regex[i + 1];
            if (next == 'd')
            {
                result += "[0-9]";
                i += 2;
            }
            else if (next == 'w')
            {
                result += "[a-zA-Z0-9_]";
                i += 2;
            }
            else if (next == 's')
            {
                result += "[ \\t\\n\\r]";
                i += 2;
            }
            else
            {
                // 保留转义序列
                result += regex[i];
                result += regex[i + 1];
                i += 2;
            }
        }
        else if (regex[i] == '[')
        {
            // 字符类，直接复制到结束
            while (i < regex.size() && regex[i] != ']')
            {
                result += regex[i];
                i++;
            }
            if (i < regex.size())
            {
                result += regex[i]; // ']'
                i++;
            }
        }
        else
        {
            result += regex[i];
            i++;
        }
    }

    return result;
}

// 判断是否是操作符
static bool isOperator(char c)
{
    return c == '|' || c == '*' || c == '+' || c == '?' || c == '.' || c == '(' || c == ')';
}

// 获取操作符优先级
static int getPrecedence(char op)
{
    switch (op)
    {
    case '|':
        return 1;
    case '.':
        return 2; // 连接
    case '*':
    case '+':
    case '?':
        return 3;
    default:
        return 0;
    }
}

// 插入显式连接符并转为后缀表达式
std::string LexerGenerator::regexToPostfix(const std::string &regex)
{
    std::string processed = preprocessRegex(regex);
    std::string withConcat;

    // 插入显式连接符 '.'
    for (size_t i = 0; i < processed.size(); i++)
    {
        char c = processed[i];
        withConcat += c;

        if (i + 1 < processed.size())
        {
            char next = processed[i + 1];

            // 当前字符后需要连接符的情况
            bool needConcat = false;

            if (c == '\\')
            {
                // 转义序列，跳过下一个字符
                i++;
                if (i < processed.size())
                {
                    withConcat += processed[i];
                    if (i + 1 < processed.size())
                    {
                        next = processed[i + 1];
                        needConcat = (next != ')' && next != '|' && next != '*' && next != '+' && next != '?');
                    }
                }
            }
            else if (c == '[')
            {
                // 字符类，找到 ']'
                while (i + 1 < processed.size() && processed[i] != ']')
                {
                    i++;
                    withConcat += processed[i];
                }
                if (i + 1 < processed.size())
                {
                    next = processed[i + 1];
                    needConcat = (next != ')' && next != '|' && next != '*' && next != '+' && next != '?');
                }
            }
            else if (c != '(' && c != '|')
            {
                needConcat = (next != ')' && next != '|' && next != '*' && next != '+' && next != '?');
            }

            if (needConcat)
            {
                withConcat += '.';
            }
        }
    }

    // 调度场算法转后缀
    std::string postfix;
    std::stack<char> opStack;

    for (size_t i = 0; i < withConcat.size(); i++)
    {
        char c = withConcat[i];

        if (c == '\\' && i + 1 < withConcat.size())
        {
            // 转义字符作为操作数
            postfix += c;
            postfix += withConcat[++i];
        }
        else if (c == '[')
        {
            // 字符类作为操作数
            postfix += c;
            while (i + 1 < withConcat.size() && withConcat[i] != ']')
            {
                i++;
                postfix += withConcat[i];
            }
        }
        else if (c == '(')
        {
            opStack.push(c);
        }
        else if (c == ')')
        {
            while (!opStack.empty() && opStack.top() != '(')
            {
                postfix += opStack.top();
                opStack.pop();
            }
            if (!opStack.empty())
                opStack.pop(); // 弹出 '('
        }
        else if (c == '|' || c == '.' || c == '*' || c == '+' || c == '?')
        {
            while (!opStack.empty() && opStack.top() != '(' &&
                   getPrecedence(opStack.top()) >= getPrecedence(c))
            {
                postfix += opStack.top();
                opStack.pop();
            }
            opStack.push(c);
        }
        else
        {
            // 普通字符
            postfix += c;
        }
    }

    while (!opStack.empty())
    {
        postfix += opStack.top();
        opStack.pop();
    }

    return postfix;
}

// 创建基本 NFA（匹配单个字符）
static NFA createBasicNFA(char c, int &nextStateID)
{
    NFA nfa;
    int start = nextStateID++;
    int end = nextStateID++;

    nfa.startState = start;
    nfa.endState = end;

    NFAState startState;
    startState.id = start;
    startState.isFinal = false;
    startState.transitions[c].insert(end);

    NFAState endState;
    endState.id = end;
    endState.isFinal = true;

    nfa.states[start] = startState;
    nfa.states[end] = endState;

    return nfa;
}

// 创建字符类 NFA（如 [a-z]）
static NFA createCharClassNFA(const std::string &charClass, int &nextStateID)
{
    NFA nfa;
    int start = nextStateID++;
    int end = nextStateID++;

    nfa.startState = start;
    nfa.endState = end;

    NFAState startState;
    startState.id = start;
    startState.isFinal = false;

    // 解析字符类
    std::set<char> chars;
    for (size_t i = 0; i < charClass.size(); i++)
    {
        if (i + 2 < charClass.size() && charClass[i + 1] == '-')
        {
            // 范围 a-z
            char from = charClass[i];
            char to = charClass[i + 2];
            for (char c = from; c <= to; c++)
            {
                chars.insert(c);
            }
            i += 2;
        }
        else if (charClass[i] == '\\' && i + 1 < charClass.size())
        {
            // 转义字符
            char next = charClass[i + 1];
            if (next == 't')
                chars.insert('\t');
            else if (next == 'n')
                chars.insert('\n');
            else if (next == 'r')
                chars.insert('\r');
            else
                chars.insert(next);
            i++;
        }
        else
        {
            chars.insert(charClass[i]);
        }
    }

    for (char c : chars)
    {
        startState.transitions[c].insert(end);
    }

    NFAState endState;
    endState.id = end;
    endState.isFinal = true;

    nfa.states[start] = startState;
    nfa.states[end] = endState;

    return nfa;
}

// NFA 连接
static NFA concatenateNFA(NFA &nfa1, NFA &nfa2)
{
    NFA result;
    result.startState = nfa1.startState;
    result.endState = nfa2.endState;

    // 复制 nfa1 的状态
    for (auto &p : nfa1.states)
    {
        result.states[p.first] = p.second;
    }

    // 复制 nfa2 的状态
    for (auto &p : nfa2.states)
    {
        result.states[p.first] = p.second;
    }

    // nfa1 的终态通过 epsilon 连接到 nfa2 的起始态
    result.states[nfa1.endState].isFinal = false;
    result.states[nfa1.endState].epsilonTransitions.insert(nfa2.startState);

    return result;
}

// NFA 并联 (|)
static NFA alternateNFA(NFA &nfa1, NFA &nfa2, int &nextStateID)
{
    NFA result;
    int newStart = nextStateID++;
    int newEnd = nextStateID++;

    result.startState = newStart;
    result.endState = newEnd;

    // 创建新的起始状态
    NFAState startState;
    startState.id = newStart;
    startState.isFinal = false;
    startState.epsilonTransitions.insert(nfa1.startState);
    startState.epsilonTransitions.insert(nfa2.startState);

    result.states[newStart] = startState;

    // 复制两个 NFA 的状态
    for (auto &p : nfa1.states)
    {
        result.states[p.first] = p.second;
    }
    for (auto &p : nfa2.states)
    {
        result.states[p.first] = p.second;
    }

    // 原来的终态连接到新终态
    result.states[nfa1.endState].isFinal = false;
    result.states[nfa1.endState].epsilonTransitions.insert(newEnd);
    result.states[nfa2.endState].isFinal = false;
    result.states[nfa2.endState].epsilonTransitions.insert(newEnd);

    // 新终态
    NFAState endState;
    endState.id = newEnd;
    endState.isFinal = true;
    result.states[newEnd] = endState;

    return result;
}

// NFA 闭包 (*)
static NFA kleeneStarNFA(NFA &nfa, int &nextStateID)
{
    NFA result;
    int newStart = nextStateID++;
    int newEnd = nextStateID++;

    result.startState = newStart;
    result.endState = newEnd;

    // 新起始状态
    NFAState startState;
    startState.id = newStart;
    startState.isFinal = false;
    startState.epsilonTransitions.insert(nfa.startState);
    startState.epsilonTransitions.insert(newEnd); // 可以直接跳过

    result.states[newStart] = startState;

    // 复制原 NFA 状态
    for (auto &p : nfa.states)
    {
        result.states[p.first] = p.second;
    }

    // 原终态连接回起始和新终态
    result.states[nfa.endState].isFinal = false;
    result.states[nfa.endState].epsilonTransitions.insert(nfa.startState);
    result.states[nfa.endState].epsilonTransitions.insert(newEnd);

    // 新终态
    NFAState endState;
    endState.id = newEnd;
    endState.isFinal = true;
    result.states[newEnd] = endState;

    return result;
}

// NFA 正闭包 (+)
static NFA plusClosureNFA(NFA &nfa, int &nextStateID)
{
    NFA result;
    int newStart = nextStateID++;
    int newEnd = nextStateID++;

    result.startState = newStart;
    result.endState = newEnd;

    // 新起始状态
    NFAState startState;
    startState.id = newStart;
    startState.isFinal = false;
    startState.epsilonTransitions.insert(nfa.startState);

    result.states[newStart] = startState;

    // 复制原 NFA 状态
    for (auto &p : nfa.states)
    {
        result.states[p.first] = p.second;
    }

    // 原终态连接回起始和新终态（至少匹配一次）
    result.states[nfa.endState].isFinal = false;
    result.states[nfa.endState].epsilonTransitions.insert(nfa.startState);
    result.states[nfa.endState].epsilonTransitions.insert(newEnd);

    // 新终态
    NFAState endState;
    endState.id = newEnd;
    endState.isFinal = true;
    result.states[newEnd] = endState;

    return result;
}

// NFA 可选 (?)
static NFA optionalNFA(NFA &nfa, int &nextStateID)
{
    NFA result;
    int newStart = nextStateID++;
    int newEnd = nextStateID++;

    result.startState = newStart;
    result.endState = newEnd;

    // 新起始状态
    NFAState startState;
    startState.id = newStart;
    startState.isFinal = false;
    startState.epsilonTransitions.insert(nfa.startState);
    startState.epsilonTransitions.insert(newEnd); // 可以跳过

    result.states[newStart] = startState;

    // 复制原 NFA 状态
    for (auto &p : nfa.states)
    {
        result.states[p.first] = p.second;
    }

    // 原终态连接到新终态
    result.states[nfa.endState].isFinal = false;
    result.states[nfa.endState].epsilonTransitions.insert(newEnd);

    // 新终态
    NFAState endState;
    endState.id = newEnd;
    endState.isFinal = true;
    result.states[newEnd] = endState;

    return result;
}

NFA LexerGenerator::regexToNFA(const std::string &regex, const std::string &tokenName)
{
    std::string postfix = regexToPostfix(regex);
    std::stack<NFA> nfaStack;

    for (size_t i = 0; i < postfix.size(); i++)
    {
        char c = postfix[i];

        if (c == '\\' && i + 1 < postfix.size())
        {
            // 转义字符
            char next = postfix[++i];
            char actual;
            if (next == 't')
                actual = '\t';
            else if (next == 'n')
                actual = '\n';
            else if (next == 'r')
                actual = '\r';
            else
                actual = next;

            nfaStack.push(createBasicNFA(actual, nextStateID));
        }
        else if (c == '[')
        {
            // 字符类
            std::string charClass;
            i++; // 跳过 '['
            while (i < postfix.size() && postfix[i] != ']')
            {
                charClass += postfix[i];
                i++;
            }
            nfaStack.push(createCharClassNFA(charClass, nextStateID));
        }
        else if (c == '.')
        {
            // 连接
            if (nfaStack.size() < 2)
                continue;
            NFA nfa2 = nfaStack.top();
            nfaStack.pop();
            NFA nfa1 = nfaStack.top();
            nfaStack.pop();
            nfaStack.push(concatenateNFA(nfa1, nfa2));
        }
        else if (c == '|')
        {
            // 并联
            if (nfaStack.size() < 2)
                continue;
            NFA nfa2 = nfaStack.top();
            nfaStack.pop();
            NFA nfa1 = nfaStack.top();
            nfaStack.pop();
            nfaStack.push(alternateNFA(nfa1, nfa2, nextStateID));
        }
        else if (c == '*')
        {
            // 闭包
            if (nfaStack.empty())
                continue;
            NFA nfa = nfaStack.top();
            nfaStack.pop();
            nfaStack.push(kleeneStarNFA(nfa, nextStateID));
        }
        else if (c == '+')
        {
            // 正闭包
            if (nfaStack.empty())
                continue;
            NFA nfa = nfaStack.top();
            nfaStack.pop();
            nfaStack.push(plusClosureNFA(nfa, nextStateID));
        }
        else if (c == '?')
        {
            // 可选
            if (nfaStack.empty())
                continue;
            NFA nfa = nfaStack.top();
            nfaStack.pop();
            nfaStack.push(optionalNFA(nfa, nextStateID));
        }
        else
        {
            // 普通字符
            nfaStack.push(createBasicNFA(c, nextStateID));
        }
    }

    NFA result;
    if (!nfaStack.empty())
    {
        result = nfaStack.top();
        // 设置终态的 token 名称
        result.states[result.endState].tokenName = tokenName;
    }

    return result;
}

NFA LexerGenerator::mergeNFAs(const std::vector<NFA> &nfas)
{
    if (nfas.empty())
        return NFA();
    if (nfas.size() == 1)
        return nfas[0];

    NFA result;
    int newStart = nextStateID++;

    result.startState = newStart;

    NFAState startState;
    startState.id = newStart;
    startState.isFinal = false;

    // 新起始状态通过 epsilon 连接到所有 NFA 的起始状态
    for (const auto &nfa : nfas)
    {
        startState.epsilonTransitions.insert(nfa.startState);

        // 复制所有状态
        for (const auto &p : nfa.states)
        {
            result.states[p.first] = p.second;
        }
    }

    result.states[newStart] = startState;

    // 合并后没有单一终态，多个终态保留各自的 tokenName
    result.endState = -1;

    return result;
}

std::set<int> LexerGenerator::epsilonClosure(const NFA &nfa, const std::set<int> &states)
{
    std::set<int> closure = states;
    std::stack<int> worklist;

    for (int s : states)
    {
        worklist.push(s);
    }

    while (!worklist.empty())
    {
        int state = worklist.top();
        worklist.pop();

        auto it = nfa.states.find(state);
        if (it != nfa.states.end())
        {
            for (int next : it->second.epsilonTransitions)
            {
                if (closure.find(next) == closure.end())
                {
                    closure.insert(next);
                    worklist.push(next);
                }
            }
        }
    }

    return closure;
}

std::set<int> LexerGenerator::move(const NFA &nfa, const std::set<int> &states, char c)
{
    std::set<int> result;

    for (int state : states)
    {
        auto it = nfa.states.find(state);
        if (it != nfa.states.end())
        {
            auto transIt = it->second.transitions.find(c);
            if (transIt != it->second.transitions.end())
            {
                for (int next : transIt->second)
                {
                    result.insert(next);
                }
            }
        }
    }

    return result;
}

void LexerGenerator::nfaToDFA(const NFA &nfa)
{
    std::map<int, DFASubset> dfaStates;
    std::map<std::set<int>, int> stateSetToID;
    std::queue<std::set<int>> worklist;
    int dfaStateCounter = 0;

    // 收集所有输入字符
    std::set<char> alphabet;
    for (const auto &p : nfa.states)
    {
        for (const auto &t : p.second.transitions)
        {
            alphabet.insert(t.first);
        }
    }

    // 初始状态：起始状态的 epsilon 闭包
    std::set<int> startSet = epsilonClosure(nfa, {nfa.startState});
    stateSetToID[startSet] = dfaStateCounter;

    DFASubset startSubset;
    startSubset.nfaStates = startSet;
    startSubset.dfaStateID = dfaStateCounter++;
    startSubset.isFinal = false;
    startSubset.tokenName = "";

    // 检查是否包含终态，选择优先级最高的（规则定义顺序靠前的）
    for (int s : startSet)
    {
        auto it = nfa.states.find(s);
        if (it != nfa.states.end() && it->second.isFinal)
        {
            startSubset.isFinal = true;
            if (startSubset.tokenName.empty() ||
                !it->second.tokenName.empty())
            {
                // 优先选择先定义的规则（tokenName 非空的）
                if (startSubset.tokenName.empty())
                {
                    startSubset.tokenName = it->second.tokenName;
                }
            }
        }
    }

    dfaStates[startSubset.dfaStateID] = startSubset;
    worklist.push(startSet);

    while (!worklist.empty())
    {
        std::set<int> currentSet = worklist.front();
        worklist.pop();

        int currentDFAState = stateSetToID[currentSet];

        for (char c : alphabet)
        {
            std::set<int> nextSet = epsilonClosure(nfa, move(nfa, currentSet, c));

            if (nextSet.empty())
                continue;

            int nextDFAState;
            if (stateSetToID.find(nextSet) == stateSetToID.end())
            {
                // 新状态
                nextDFAState = dfaStateCounter++;
                stateSetToID[nextSet] = nextDFAState;

                DFASubset newSubset;
                newSubset.nfaStates = nextSet;
                newSubset.dfaStateID = nextDFAState;
                newSubset.isFinal = false;
                newSubset.tokenName = "";

                // 检查终态，按规则优先级选择
                for (const auto &rule : rules)
                {
                    for (int s : nextSet)
                    {
                        auto it = nfa.states.find(s);
                        if (it != nfa.states.end() && it->second.isFinal &&
                            it->second.tokenName == rule.name)
                        {
                            newSubset.isFinal = true;
                            if (newSubset.tokenName.empty())
                            {
                                newSubset.tokenName = it->second.tokenName;
                            }
                            break;
                        }
                    }
                    if (!newSubset.tokenName.empty())
                        break;
                }

                dfaStates[nextDFAState] = newSubset;
                worklist.push(nextSet);
            }
            else
            {
                nextDFAState = stateSetToID[nextSet];
            }

            dfaStates[currentDFAState].transitions[c] = nextDFAState;
        }
    }

    convertToDFATable(dfaStates);
}

void LexerGenerator::minimizeDFA()
{
    // 简化版 Hopcroft 算法
    if (dfaTable.empty())
        return;

    // 收集所有字符
    std::set<char> alphabet;
    for (const auto &row : dfaTable)
    {
        for (const auto &t : row.transitions)
        {
            alphabet.insert(t.first);
        }
    }

    // 初始划分：终态按 tokenName 分组，非终态一组
    std::map<std::string, std::set<int>> partitions;
    std::set<int> nonFinalStates;

    for (const auto &row : dfaTable)
    {
        if (row.isFinal)
        {
            partitions[row.tokenName].insert(row.stateID);
        }
        else
        {
            nonFinalStates.insert(row.stateID);
        }
    }

    if (!nonFinalStates.empty())
    {
        partitions["__NONFINAL__"] = nonFinalStates;
    }

    // 将划分转为向量形式
    std::vector<std::set<int>> P;
    for (const auto &p : partitions)
    {
        P.push_back(p.second);
    }

    // 迭代细化
    bool changed = true;
    while (changed)
    {
        changed = false;
        std::vector<std::set<int>> newP;

        for (const auto &group : P)
        {
            if (group.size() <= 1)
            {
                newP.push_back(group);
                continue;
            }

            // 尝试按转换拆分
            std::map<std::vector<int>, std::set<int>> subgroups;

            for (int state : group)
            {
                std::vector<int> signature;
                for (char c : alphabet)
                {
                    int target = -1;
                    auto it = dfaTable[state].transitions.find(c);
                    if (it != dfaTable[state].transitions.end())
                    {
                        int targetState = it->second;
                        // 找目标所在的组
                        for (size_t i = 0; i < P.size(); i++)
                        {
                            if (P[i].find(targetState) != P[i].end())
                            {
                                target = (int)i;
                                break;
                            }
                        }
                    }
                    signature.push_back(target);
                }
                subgroups[signature].insert(state);
            }

            if (subgroups.size() > 1)
            {
                changed = true;
            }

            for (const auto &sg : subgroups)
            {
                newP.push_back(sg.second);
            }
        }

        P = newP;
    }

    // 根据划分重建 DFA
    if (P.size() == dfaTable.size())
    {
        return; // 无法进一步最小化
    }

    std::map<int, int> oldToNew;
    for (size_t i = 0; i < P.size(); i++)
    {
        for (int state : P[i])
        {
            oldToNew[state] = (int)i;
        }
    }

    DFATable newTable;
    for (size_t i = 0; i < P.size(); i++)
    {
        int representative = *P[i].begin();
        DFARow newRow;
        newRow.stateID = (int)i;
        newRow.isFinal = dfaTable[representative].isFinal;
        newRow.tokenName = dfaTable[representative].tokenName;

        for (const auto &t : dfaTable[representative].transitions)
        {
            newRow.transitions[t.first] = oldToNew[t.second];
        }

        newTable.push_back(newRow);
    }

    dfaTable = newTable;
}

void LexerGenerator::convertToDFATable(const std::map<int, DFASubset> &dfaStates)
{
    dfaTable.clear();

    // 确保状态 ID 连续
    std::map<int, int> idMap;
    int newID = 0;
    for (const auto &p : dfaStates)
    {
        idMap[p.first] = newID++;
    }

    dfaTable.resize(dfaStates.size());

    for (const auto &p : dfaStates)
    {
        int idx = idMap[p.first];
        dfaTable[idx].stateID = idx;
        dfaTable[idx].isFinal = p.second.isFinal;
        dfaTable[idx].tokenName = p.second.tokenName;

        for (const auto &t : p.second.transitions)
        {
            dfaTable[idx].transitions[t.first] = idMap[t.second];
        }
    }
}
