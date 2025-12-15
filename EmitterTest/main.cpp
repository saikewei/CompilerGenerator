#pragma once

#include "CompilerGenerator/Types.h"
#include "CompilerGenerator/CodeEmitter.h"
#include <iostream>

// 辅助函数：给一行 DFA 添加一个字符范围的跳转
// 例如：状态 0 遇到 'a'到'z' 都跳转到 状态 2
static void addRange(DFARow & row, char start, char end, int targetState) {
    for (char c = start; c <= end; ++c) {
        row.transitions[c] = targetState;
    }
}

// === 核心 Mock 函数 ===
DFATable createMockDFA() {
    DFATable dfa;

    // --- 状态 0: 初始状态 (Start State) ---
    // 逻辑：
    // - 遇到 '0'-'9' -> 去状态 1 (开始识别数字)
    // - 遇到 'a'-'z' -> 去状态 2 (开始识别变量)
    // - 遇到 '+'     -> 去状态 3 (识别加号)
    // - 遇到 ' '     -> 留状态 0 (跳过空格)
    {
        DFARow row0;
        row0.stateID = 0;
        row0.isFinal = false; // 初始状态通常不是终态

        addRange(row0, '0', '9', 1);
        addRange(row0, 'a', 'z', 2);
        row0.transitions['+'] = 3;
        row0.transitions[' '] = 0; // 自循环以消耗连续空格

        dfa.push_back(row0);
    }

    // --- 状态 1: 数字状态 (NUM) ---
    // 逻辑：
    // - 遇到 '0'-'9' -> 留状态 1 (继续读数字)
    // - 其他字符     -> 没有转换 (Lexer 循环会结束，接受 NUM)
    {
        DFARow row1;
        row1.stateID = 1;
        row1.isFinal = true;
        row1.tokenName = "NUM"; // 如果在这里停下，就是 NUM

        addRange(row1, '0', '9', 1); // 允许 "123"

        dfa.push_back(row1);
    }

    // --- 状态 2: 标识符状态 (ID) ---
    // 逻辑：
    // - 遇到 'a'-'z' -> 留状态 2
    {
        DFARow row2;
        row2.stateID = 2;
        row2.isFinal = true;
        row2.tokenName = "ID";

        addRange(row2, 'a', 'z', 2); // 允许 "abc"

        dfa.push_back(row2);
    }

    // --- 状态 3: 加号状态 (PLUS) ---
    // 逻辑：
    // - '+' 只有一个字符，后面没有什么可接的了，所以没有出边
    {
        DFARow row3;
        row3.stateID = 3;
        row3.isFinal = true;
        row3.tokenName = "PLUS";

        // 没有 transitions，意味着读完 + 就必须接受了

        dfa.push_back(row3);
    }

    return dfa;
}

int main() {
	DFATable dfa = createMockDFA();

	CodeEmitter emitter("output");
	emitter.emitLexer("lexer.cpp", dfa);
    return 0;
}
