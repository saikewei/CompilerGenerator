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

// 辅助：批量添加字符范围跳转
static void addRange(DFARow& row, char start, char end, int targetState) {
    for (char c = start; c <= end; ++c) {
        row.transitions[c] = targetState;
    }
}

// ==========================================
// 2. 构建 DFA (词法分析器数据)
// ==========================================
// Token 支持: NUM (123), ID (abc), PLUS (+), MUL (*), ASSIGN (=), LPAREN ((), RPAREN ())
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
        r.transitions[' '] = 0;   // Skip Space
        dfa.push_back(r);
    }

    // --- Final States ---
    // State 1: NUM
    { DFARow r; r.stateID = 1; r.isFinal = true; r.tokenName = "NUM"; addRange(r, '0', '9', 1); dfa.push_back(r); }
    // State 2: ID
    { DFARow r; r.stateID = 2; r.isFinal = true; r.tokenName = "ID"; addRange(r, 'a', 'z', 2); dfa.push_back(r); }
    // State 3: PLUS
    { DFARow r; r.stateID = 3; r.isFinal = true; r.tokenName = "PLUS"; dfa.push_back(r); }
    // State 4: MUL
    { DFARow r; r.stateID = 4; r.isFinal = true; r.tokenName = "MUL"; dfa.push_back(r); }
    // State 5: ASSIGN
    { DFARow r; r.stateID = 5; r.isFinal = true; r.tokenName = "ASSIGN"; dfa.push_back(r); }
    // State 6: LPAREN
    { DFARow r; r.stateID = 6; r.isFinal = true; r.tokenName = "LPAREN"; dfa.push_back(r); }
    // State 7: RPAREN
    { DFARow r; r.stateID = 7; r.isFinal = true; r.tokenName = "RPAREN"; dfa.push_back(r); }

    return dfa;
}

// ==========================================
// 3. 构建 Grammar Rules (产生式与语义动作)
// ==========================================
// 语法结构:
// S -> ID ASSIGN E
// E -> E PLUS T | T
// T -> T MUL F  | F
// F -> LPAREN E RPAREN | NUM | ID
std::vector<ProductionRule> createMockRules() {
    std::vector<ProductionRule> rules;

    // Rule 0: S' -> S (Augmented start symbol)
    rules.push_back({ 0, "S'", {"S"}, "" });

    // Rule 1: S -> ID ASSIGN E
    // 动作: 输出最终赋值代码
    rules.push_back({ 1, "S", {"ID", "ASSIGN", "E"},
        "emit($1.text + \" = \" + $3.var);"
        });

    // Rule 2: E -> E PLUS T
    // 动作: 加法生成临时变量
    rules.push_back({ 2, "E", {"E", "PLUS", "T"},
        "$$.var = newTemp();\n"
        "            emit($$.var + \" = \" + $1.var + \" + \" + $3.var);"
        });

    // Rule 3: E -> T
    // 动作: 传递属性
    rules.push_back({ 3, "E", {"T"},
        "$$.var = $1.var;"
        });

    // Rule 4: T -> T MUL F
    // 动作: 乘法生成临时变量 (优先级高于加法)
    rules.push_back({ 4, "T", {"T", "MUL", "F"},
        "$$.var = newTemp();\n"
        "            emit($$.var + \" = \" + $1.var + \" * \" + $3.var);"
        });

    // Rule 5: T -> F
    rules.push_back({ 5, "T", {"F"},
        "$$.var = $1.var;"
        });

    // Rule 6: F -> LPAREN E RPAREN
    // 动作: 把括号里算出来的 E 的结果 ($2) 传给 F ($$)
    rules.push_back({ 6, "F", {"LPAREN", "E", "RPAREN"},
        "$$.var = $2.var;"
        });

    // Rule 7: F -> NUM
    rules.push_back({ 7, "F", {"NUM"},
        "$$.var = $1.text;" // 直接使用数字字面量
        });

    // Rule 8: F -> ID
    rules.push_back({ 8, "F", {"ID"},
        "$$.var = $1.text;" // 直接使用变量名
        });

    return rules;
}

// ==========================================
// 4. 构建 LR 分析表 (核心逻辑)
// ==========================================
// 为了支持优先级，这个表是精心构造的
void createMockParserTables(ActionTable& actionTbl, GotoTable& gotoTbl) {

    // --- State 0: Start. Expect "ID ASSIGN ..." ---
    actionTbl[{0, "ID"}] = { ACTION_SHIFT, 2 };
    gotoTbl[{0, "S"}] = 1;

    // --- State 1: Accept ---
    actionTbl[{1, "EOF"}] = { ACTION_ACCEPT, 0 };

    // --- State 2: Seen "ID". Expect "ASSIGN" ---
    actionTbl[{2, "ASSIGN"}] = { ACTION_SHIFT, 3 };

    // --- State 3: Seen "ID =". Expect Expression Start (NUM, ID, LPAREN) ---
    // 这是表达式解析的入口状态
    actionTbl[{3, "NUM"}] = { ACTION_SHIFT, 7 };
    actionTbl[{3, "ID"}] = { ACTION_SHIFT, 8 };
    actionTbl[{3, "LPAREN"}] = { ACTION_SHIFT, 11 };
    gotoTbl[{3, "E"}] = 4;
    gotoTbl[{3, "T"}] = 5;
    gotoTbl[{3, "F"}] = 6;

    // --- State 4: Seen "E". Expect "PLUS" or End ---
    actionTbl[{4, "PLUS"}] = { ACTION_SHIFT, 9 };
    actionTbl[{4, "EOF"}] = { ACTION_REDUCE, 1 }; // S -> ID ASSIGN E
    // 注意：如果这里遇到 RPAREN，说明括号不匹配，因为这是顶层 E

    // --- State 5: Seen "T". Expect "MUL", "PLUS" or End ---
    actionTbl[{5, "MUL"}] = { ACTION_SHIFT, 10 }; // 移进乘号 (优先级高)
    actionTbl[{5, "PLUS"}] = { ACTION_REDUCE, 3 }; // 遇到加号，规约 E->T
    actionTbl[{5, "EOF"}] = { ACTION_REDUCE, 3 };
    actionTbl[{5, "RPAREN"}] = { ACTION_REDUCE, 3 };

    // --- State 6: Seen "F". Always Reduce T->F ---
    actionTbl[{6, "MUL"}] = { ACTION_REDUCE, 5 };
    actionTbl[{6, "PLUS"}] = { ACTION_REDUCE, 5 };
    actionTbl[{6, "EOF"}] = { ACTION_REDUCE, 5 };
    actionTbl[{6, "RPAREN"}] = { ACTION_REDUCE, 5 };

    // --- State 7: Seen "NUM". Reduce F->NUM ---
    actionTbl[{7, "MUL"}] = { ACTION_REDUCE, 7 };
    actionTbl[{7, "PLUS"}] = { ACTION_REDUCE, 7 };
    actionTbl[{7, "EOF"}] = { ACTION_REDUCE, 7 };
    actionTbl[{7, "RPAREN"}] = { ACTION_REDUCE, 7 };

    // --- State 8: Seen "ID" (in expr). Reduce F->ID ---
    actionTbl[{8, "MUL"}] = { ACTION_REDUCE, 8 };
    actionTbl[{8, "PLUS"}] = { ACTION_REDUCE, 8 };
    actionTbl[{8, "EOF"}] = { ACTION_REDUCE, 8 };
    actionTbl[{8, "RPAREN"}] = { ACTION_REDUCE, 8 };

    // --- State 9: Seen "E PLUS". Expect "T" (NUM, ID, LPAREN) ---
    actionTbl[{9, "NUM"}] = { ACTION_SHIFT, 7 };
    actionTbl[{9, "ID"}] = { ACTION_SHIFT, 8 };
    actionTbl[{9, "LPAREN"}] = { ACTION_SHIFT, 11 };
    gotoTbl[{9, "T"}] = 14;
    gotoTbl[{9, "F"}] = 6;

    // --- State 10: Seen "T MUL". Expect "F" (NUM, ID, LPAREN) ---
    actionTbl[{10, "NUM"}] = { ACTION_SHIFT, 7 };
    actionTbl[{10, "ID"}] = { ACTION_SHIFT, 8 };
    actionTbl[{10, "LPAREN"}] = { ACTION_SHIFT, 11 };
    gotoTbl[{10, "F"}] = 15;

    // --- State 11: Seen "LPAREN". Expect Expression ---
    // 类似于 State 3，但后续要去寻找 RPAREN
    actionTbl[{11, "NUM"}] = { ACTION_SHIFT, 7 };
    actionTbl[{11, "ID"}] = { ACTION_SHIFT, 8 };
    actionTbl[{11, "LPAREN"}] = { ACTION_SHIFT, 11 }; // 递归嵌套 ((...))
    gotoTbl[{11, "E"}] = 12;
    gotoTbl[{11, "T"}] = 5;
    gotoTbl[{11, "F"}] = 6;

    // --- State 12: Seen "( E". Expect "RPAREN" or "PLUS" ---
    actionTbl[{12, "PLUS"}] = { ACTION_SHIFT, 9 };
    actionTbl[{12, "RPAREN"}] = { ACTION_SHIFT, 13 }; // 移进右括号

    // --- State 13: Seen "( E )". Reduce F -> ( E ) ---
    actionTbl[{13, "MUL"}] = { ACTION_REDUCE, 6 };
    actionTbl[{13, "PLUS"}] = { ACTION_REDUCE, 6 };
    actionTbl[{13, "EOF"}] = { ACTION_REDUCE, 6 };
    actionTbl[{13, "RPAREN"}] = { ACTION_REDUCE, 6 };

    // --- State 14: Seen "E PLUS T". Reduce E -> E PLUS T ---
    // 关键点：如果后面是 MUL，则移进 (优先级)；如果是 PLUS/EOF/) 则规约
    actionTbl[{14, "MUL"}] = { ACTION_SHIFT, 10 }; // 移进乘法
    actionTbl[{14, "PLUS"}] = { ACTION_REDUCE, 2 }; // 规约加法
    actionTbl[{14, "EOF"}] = { ACTION_REDUCE, 2 };
    actionTbl[{14, "RPAREN"}] = { ACTION_REDUCE, 2 };

    // --- State 15: Seen "T MUL F". Reduce T -> T MUL F ---
    actionTbl[{15, "MUL"}] = { ACTION_REDUCE, 4 };
    actionTbl[{15, "PLUS"}] = { ACTION_REDUCE, 4 };
    actionTbl[{15, "EOF"}] = { ACTION_REDUCE, 4 };
    actionTbl[{15, "RPAREN"}] = { ACTION_REDUCE, 4 };
}

// ==========================================
// 5. Main Execution
// ==========================================
int main() {
    std::cout << "[1/4] Preparing Mock Data..." << std::endl;

    DFATable dfa = createMockDFA();
    std::vector<ProductionRule> rules = createMockRules();

    ActionTable actionTbl;
    GotoTable gotoTbl;
    createMockParserTables(actionTbl, gotoTbl);

    std::cout << "[2/4] Initializing CodeEmitter..." << std::endl;
    // 使用当前目录作为输出
    CodeEmitter emitter("output");

    std::cout << "[3/4] Generating Lexer..." << std::endl;
    if (emitter.emitLexer(dfa)) {
        std::cout << "  -> Success: Lexer.h / Lexer.cpp created." << std::endl;
    }
    else {
        std::cerr << "  -> Failed to generate Lexer!" << std::endl;
        return 1;
    }

    std::cout << "[4/4] Generating Parser..." << std::endl;
    if (emitter.emitParser(actionTbl, gotoTbl, rules)) {
        std::cout << "  -> Success: Parser.h / Parser.cpp created." << std::endl;
    }
    else {
        std::cerr << "  -> Failed to generate Parser!" << std::endl;
        return 1;
    }

    std::cout << "\n=============================================" << std::endl;
    std::cout << "Done! Now compile the generated files with:" << std::endl;
    std::cout << "   g++ output/Lexer.cpp output/Parser.cpp main_test.cpp -o compiler" << std::endl;
    std::cout << "   (Make sure you have a main_test.cpp to run the generated classes)" << std::endl;
    std::cout << "=============================================" << std::endl;

    return 0;
}