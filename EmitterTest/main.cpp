#pragma once

#include "CompilerGenerator/Types.h"
#include "CompilerGenerator/CodeEmitter.h"
#include <iostream>
#include <vector>
#include <map>
#include <string>

// ==========================================
// 1. 辅助工具
// ==========================================

static void addRange(DFARow& row, char start, char end, int targetState) {
    for (char c = start; c <= end; ++c) {
        row.transitions[c] = targetState;
    }
}

// ==========================================
// 2. 构建 DFA (词法分析器数据)
// ==========================================
// 新增: ';' (SEMI)
DFATable createMockDFA() {
    DFATable dfa;

    // --- State 0: Start State ---
    {
        DFARow r; r.stateID = 0; r.isFinal = false;
        addRange(r, '0', '9', 1); // NUM
        addRange(r, 'a', 'z', 2); // ID
        r.transitions['+'] = 3;   // PLUS
        r.transitions['*'] = 4;   // MUL
        r.transitions['='] = 5;   // ASSIGN
        r.transitions['('] = 6;   // LPAREN
        r.transitions[')'] = 7;   // RPAREN
        r.transitions['?'] = 8;   // IF
        r.transitions['<'] = 9;   // RELOP
        r.transitions[';'] = 10;  // SEMI (新增)
        r.transitions[' '] = 0;   // Skip
        dfa.push_back(r);
    }

    // --- Final States ---
    { DFARow r; r.stateID = 1; r.isFinal = true; r.tokenName = "NUM"; addRange(r, '0', '9', 1); dfa.push_back(r); }
    { DFARow r; r.stateID = 2; r.isFinal = true; r.tokenName = "ID"; addRange(r, 'a', 'z', 2); dfa.push_back(r); }
    { DFARow r; r.stateID = 3; r.isFinal = true; r.tokenName = "PLUS"; dfa.push_back(r); }
    { DFARow r; r.stateID = 4; r.isFinal = true; r.tokenName = "MUL"; dfa.push_back(r); }
    { DFARow r; r.stateID = 5; r.isFinal = true; r.tokenName = "ASSIGN"; dfa.push_back(r); }
    { DFARow r; r.stateID = 6; r.isFinal = true; r.tokenName = "LPAREN"; dfa.push_back(r); }
    { DFARow r; r.stateID = 7; r.isFinal = true; r.tokenName = "RPAREN"; dfa.push_back(r); }
    { DFARow r; r.stateID = 8; r.isFinal = true; r.tokenName = "IF"; dfa.push_back(r); }
    { DFARow r; r.stateID = 9; r.isFinal = true; r.tokenName = "RELOP"; dfa.push_back(r); }
    // 新增: 分号
    { DFARow r; r.stateID = 10; r.isFinal = true; r.tokenName = "SEMI"; dfa.push_back(r); }

    return dfa;
}

// ==========================================
// 3. 构建 Grammar Rules
// ==========================================
std::vector<ProductionRule> createMockRules() {
    std::vector<ProductionRule> rules;

    // Rule 0: S' -> L (Augmented start symbol, L 代表 List)
    rules.push_back({ 0, "S'", {"L"},
        "backpatch($1.nextList, nextquad());\n"
        "            emit(\"// End of Program\");" // 可选：生成一个显式的结束标记
        });

    // Rule 1: S -> ID ASSIGN E
    rules.push_back({ 1, "S", {"ID", "ASSIGN", "E"},
        "emit($1.text + \" = \" + $3.var);"
        });

    // Rule 2: E -> E PLUS T
    rules.push_back({ 2, "E", {"E", "PLUS", "T"},
        "$$.var = newTemp();\n"
        "            emit($$.var + \" = \" + $1.var + \" + \" + $3.var);"
        });

    // Rule 3: E -> T
    rules.push_back({ 3, "E", {"T"}, "$$.var = $1.var;" });

    // Rule 4: T -> T MUL F
    rules.push_back({ 4, "T", {"T", "MUL", "F"},
        "$$.var = newTemp();\n"
        "            emit($$.var + \" = \" + $1.var + \" * \" + $3.var);"
        });

    // Rule 5: T -> F
    rules.push_back({ 5, "T", {"F"}, "$$.var = $1.var;" });

    // Rule 6: F -> LPAREN E RPAREN
    rules.push_back({ 6, "F", {"LPAREN", "E", "RPAREN"}, "$$.var = $2.var;" });

    // Rule 7: F -> NUM
    rules.push_back({ 7, "F", {"NUM"}, "$$.var = $1.text;" });

    // Rule 8: F -> ID
    rules.push_back({ 8, "F", {"ID"}, "$$.var = $1.text;" });

    // Rule 9: B -> E RELOP E
    rules.push_back({ 9, "B", {"E", "RELOP", "E"},
        "$$.trueList = makelist(nextquad());\n"
        "            $$.falseList = makelist(nextquad() + 1);\n"
        "            emit(\"if \" + $1.var + \" < \" + $3.var + \" goto\");\n"
        "            emit(\"goto\");"
        });

    // Rule 10: M -> epsilon
    rules.push_back({ 10, "M", {}, "$$.quad = nextquad();" });

    // Rule 11: S -> IF LPAREN B RPAREN M S
    rules.push_back({ 11, "S", {"IF", "LPAREN", "B", "RPAREN", "M", "S"},
        "backpatch($3.trueList, $5.quad);\n"
        "            $$.nextList = merge($3.falseList, $6.nextList);"
        });

    // --- 新增：语句列表规则 ---

    // Rule 12: L -> S
    // 逻辑：L 的 nextList 就是 S 的 nextList
    rules.push_back({ 12, "L", {"S"},
        "$$.nextList = $1.nextList;"
        });

    // Rule 13: L -> L SEMI M S
    // 逻辑：前一个 L 执行完后 (L.nextList)，应该跳到 S 的开始 (M.quad)
    rules.push_back({ 13, "L", {"L", "SEMI", "M", "S"},
        "backpatch($1.nextList, $3.quad);\n"
        "            $$.nextList = $4.nextList;"
        });

    return rules;
}

// ==========================================
// 4. 构建 LR 分析表
// ==========================================
void createMockParserTables(ActionTable& actionTbl, GotoTable& gotoTbl) {
    // === 基础层级 (State 0-24) ===
    // 我们保留大部分之前的逻辑，但在 State 0 和 State 1 做关键修改以支持 L

    // State 0: Start. 
    // 路径: . L
    //       . S
    //       . ID = ...
    //       . IF ...
    actionTbl[{0, "ID"}] = { ACTION_SHIFT, 2 };
    actionTbl[{0, "IF"}] = { ACTION_SHIFT, 16 };
    gotoTbl[{0, "S"}] = 25; // S 规约为 L
    gotoTbl[{0, "L"}] = 1;  // L 完成
    gotoTbl[{0, "S'"}] = 30;

    // State 1: Accepted L. Expect EOF or SEMI (L -> L . ; M S)
    actionTbl[{1, "EOF"}] = { ACTION_REDUCE, 0 };
    actionTbl[{1, "SEMI"}] = { ACTION_SHIFT, 26 };

    // State 25: Seen "S". Reduce L -> S (Rule 12)
    // S 后面可能跟着 EOF 或者 SEMI
    actionTbl[{25, "EOF"}] = { ACTION_REDUCE, 12 };
    actionTbl[{25, "SEMI"}] = { ACTION_REDUCE, 12 };

    // --- 状态 2-24 保持原样 (省略重复代码，直接复制之前的逻辑) ---
    // (为了代码完整性，这里必须包含之前的映射，否则生成的 switch-case 会缺页)
    actionTbl[{2, "ASSIGN"}] = { ACTION_SHIFT, 3 };
    actionTbl[{3, "NUM"}] = { ACTION_SHIFT, 7 }; actionTbl[{3, "ID"}] = { ACTION_SHIFT, 8 }; actionTbl[{3, "LPAREN"}] = { ACTION_SHIFT, 11 };
    gotoTbl[{3, "E"}] = 4; gotoTbl[{3, "T"}] = 5; gotoTbl[{3, "F"}] = 6;

    // State 4
    actionTbl[{4, "PLUS"}] = { ACTION_SHIFT, 9 };
    actionTbl[{4, "EOF"}] = { ACTION_REDUCE, 1 };
    actionTbl[{4, "SEMI"}] = { ACTION_REDUCE, 1 }; // S 结束可能是分号

    // State 5
    actionTbl[{5, "MUL"}] = { ACTION_SHIFT, 10 };
    actionTbl[{5, "PLUS"}] = { ACTION_REDUCE, 3 }; actionTbl[{5, "EOF"}] = { ACTION_REDUCE, 3 }; actionTbl[{5, "RELOP"}] = { ACTION_REDUCE, 3 }; actionTbl[{5, "RPAREN"}] = { ACTION_REDUCE, 3 };
    actionTbl[{5, "SEMI"}] = { ACTION_REDUCE, 3 }; // 增加 SEMI 规约

    // State 6, 7, 8 (规约状态增加 SEMI)
    auto addReduce = [&](int state, int rule) {
        actionTbl[{state, "MUL"}] = { ACTION_REDUCE, rule };
        actionTbl[{state, "PLUS"}] = { ACTION_REDUCE, rule };
        actionTbl[{state, "EOF"}] = { ACTION_REDUCE, rule };
        actionTbl[{state, "RELOP"}] = { ACTION_REDUCE, rule };
        actionTbl[{state, "RPAREN"}] = { ACTION_REDUCE, rule };
        actionTbl[{state, "SEMI"}] = { ACTION_REDUCE, rule }; // 新增
        };
    addReduce(6, 5); addReduce(7, 7); addReduce(8, 8);

    // State 9
    actionTbl[{9, "NUM"}] = { ACTION_SHIFT, 7 }; actionTbl[{9, "ID"}] = { ACTION_SHIFT, 8 }; actionTbl[{9, "LPAREN"}] = { ACTION_SHIFT, 11 };
    gotoTbl[{9, "T"}] = 14; gotoTbl[{9, "F"}] = 6;
    // State 10
    actionTbl[{10, "NUM"}] = { ACTION_SHIFT, 7 }; actionTbl[{10, "ID"}] = { ACTION_SHIFT, 8 }; actionTbl[{10, "LPAREN"}] = { ACTION_SHIFT, 11 };
    gotoTbl[{10, "F"}] = 15;
    // State 11
    actionTbl[{11, "NUM"}] = { ACTION_SHIFT, 7 }; actionTbl[{11, "ID"}] = { ACTION_SHIFT, 8 }; actionTbl[{11, "LPAREN"}] = { ACTION_SHIFT, 11 };
    gotoTbl[{11, "E"}] = 12; gotoTbl[{11, "T"}] = 5; gotoTbl[{11, "F"}] = 6;
    // State 12
    actionTbl[{12, "PLUS"}] = { ACTION_SHIFT, 9 }; actionTbl[{12, "RPAREN"}] = { ACTION_SHIFT, 13 };
    // State 13
    addReduce(13, 6);
    // State 14
    actionTbl[{14, "MUL"}] = { ACTION_SHIFT, 10 };
    actionTbl[{14, "PLUS"}] = { ACTION_REDUCE, 2 }; actionTbl[{14, "EOF"}] = { ACTION_REDUCE, 2 }; actionTbl[{14, "RPAREN"}] = { ACTION_REDUCE, 2 };
    actionTbl[{14, "SEMI"}] = { ACTION_REDUCE, 2 };
    // State 15
    addReduce(15, 4);

    // IF 逻辑 (States 16-24)
    actionTbl[{16, "LPAREN"}] = { ACTION_SHIFT, 17 };
    actionTbl[{17, "NUM"}] = { ACTION_SHIFT, 7 }; actionTbl[{17, "ID"}] = { ACTION_SHIFT, 8 };
    gotoTbl[{17, "E"}] = 18; gotoTbl[{17, "T"}] = 5; gotoTbl[{17, "F"}] = 6; gotoTbl[{17, "B"}] = 21; // 补全 Goto B

    actionTbl[{18, "RELOP"}] = { ACTION_SHIFT, 19 }; actionTbl[{18, "PLUS"}] = { ACTION_SHIFT, 9 };
    actionTbl[{19, "NUM"}] = { ACTION_SHIFT, 7 }; actionTbl[{19, "ID"}] = { ACTION_SHIFT, 8 };
    gotoTbl[{19, "E"}] = 20; gotoTbl[{19, "T"}] = 5; gotoTbl[{19, "F"}] = 6;

    actionTbl[{20, "RPAREN"}] = { ACTION_REDUCE, 9 }; actionTbl[{20, "PLUS"}] = { ACTION_SHIFT, 9 };
    actionTbl[{21, "RPAREN"}] = { ACTION_SHIFT, 22 };

    // State 22: IF ( B ) . M S
    // 这里也是特殊的，如果遇到 ID 或 IF (S 的开始)，就规约 M -> epsilon
    actionTbl[{22, "ID"}] = { ACTION_REDUCE, 10 };
    actionTbl[{22, "IF"}] = { ACTION_REDUCE, 10 };
    gotoTbl[{22, "M"}] = 23;

    actionTbl[{23, "ID"}] = { ACTION_SHIFT, 2 };
    actionTbl[{23, "IF"}] = { ACTION_SHIFT, 16 };
    gotoTbl[{23, "S"}] = 24;

    // State 24: S 结束
    actionTbl[{24, "EOF"}] = { ACTION_REDUCE, 11 };
    actionTbl[{24, "SEMI"}] = { ACTION_REDUCE, 11 }; // IF 语句也是一句 S，后面可能是分号


    // === 新增：多语句支持逻辑 (States 26+) ===

    // State 26: Seen "L ;". Expect M (Marker) -> S
    // M -> epsilon. Lookahead is ID or IF. Reduce Rule 10.
    actionTbl[{26, "ID"}] = { ACTION_REDUCE, 10 };
    actionTbl[{26, "IF"}] = { ACTION_REDUCE, 10 };
    gotoTbl[{26, "M"}] = 27;

    // State 27: Seen "L ; M". Expect S
    // S -> ID ... or S -> IF ...
    actionTbl[{27, "ID"}] = { ACTION_SHIFT, 2 };
    actionTbl[{27, "IF"}] = { ACTION_SHIFT, 16 };
    gotoTbl[{27, "S"}] = 28;

    // State 28: Seen "L ; M S". Reduce L -> L ; M S (Rule 13)
    // 后面可能还是分号，或者 EOF
    actionTbl[{28, "EOF"}] = { ACTION_REDUCE, 13 };
    actionTbl[{28, "SEMI"}] = { ACTION_REDUCE, 13 };

    actionTbl[{30, "EOF"}] = { ACTION_ACCEPT, 0 };
}

// ==========================================
// 5. Main Execution
// ==========================================
int main() {
    std::cout << "[1/4] Preparing Mock Data (Multi-statement Support)..." << std::endl;

    DFATable dfa = createMockDFA();
    std::vector<ProductionRule> rules = createMockRules();

    ActionTable actionTbl;
    GotoTable gotoTbl;
    createMockParserTables(actionTbl, gotoTbl);

    CodeEmitter emitter("output");

    std::cout << "[3/4] Generating Lexer..." << std::endl;
    if (emitter.emitLexer(dfa)) std::cout << "  -> OK." << std::endl;
    else return 1;

    std::cout << "[4/4] Generating Parser..." << std::endl;
    if (emitter.emitParser(actionTbl, gotoTbl, rules)) std::cout << "  -> OK." << std::endl;
    else return 1;

    std::cout << "\n=============================================" << std::endl;
    std::cout << "Test Case Plan:" << std::endl;
    std::cout << "  Input:  a = 1 ; ? ( a < 10 ) b = 2" << std::endl;
    std::cout << "  Expect: " << std::endl;
    std::cout << "     0: a = 1" << std::endl;
    std::cout << "     1: if a < 10 goto 3" << std::endl;
    std::cout << "     2: goto (Next)" << std::endl;
    std::cout << "     3: b = 2" << std::endl;
    std::cout << "=============================================" << std::endl;

    return 0;
}