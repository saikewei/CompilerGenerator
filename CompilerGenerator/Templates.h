#pragma once

#include <string>

// =========================================================
// 1. Lexer 头文件模版 (Lexer.h)
// =========================================================
const std::string TEMPLATE_LEXER_H = R"(
#ifndef GENERATED_LEXER_H
#define GENERATED_LEXER_H

#include <string>
#include <iostream>

// Token 结构定义
struct Token {
    std::string type;
    std::string text;
    int line;
};

// Lexer 类定义
class Lexer {
public:
    // 构造函数：接收源代码
    Lexer(const std::string& source);

    // 获取下一个 Token
    Token nextToken();

    // 获取当前行号（用于报错）
    int getLine() const;

private:
    std::string m_source; // 源代码
    size_t m_pos;         // 当前字符位置
    int m_line;           // 当前行号
    
    // 辅助：获取当前字符
    char peek() const;
    // 辅助：吃掉一个字符
    char advance();
};

#endif // GENERATED_LEXER_H
)";

// =========================================================
// 2. Lexer 实现文件模版 (Lexer.cpp)
// =========================================================
const std::string TEMPLATE_LEXER_CPP = R"(
#include "lexer.h"

Lexer::Lexer(const std::string& source) 
    : m_source(source), m_pos(0), m_line(1) {}

int Lexer::getLine() const {
    return m_line;
}

char Lexer::peek() const {
    if (m_pos >= m_source.length()) return '\0';
    return m_source[m_pos];
}

char Lexer::advance() {
    if (m_pos >= m_source.length()) return '\0';
    char c = m_source[m_pos];
    m_pos++;
    if (c == '\n') m_line++;
    return c;
}

Token Lexer::nextToken() {
    // 跳过 EOF 检查，由状态机处理或在此处处理
    if (m_pos >= m_source.length()) {
        return Token{"EOF", "", m_line};
    }

    int state = 0;       // 初始状态
    std::string currentText;
    
    // 记录开始时的位置，如果匹配失败方便回溯（简单DFA通常不需要回溯，这里仅作变量定义）
    // size_t startPos = m_pos; 

    // 贪婪匹配循环
    while (true) {
        char c = peek(); // 只看一眼，不移动指针
        int nextState = -1;

        // ==========================================
        //  DFA 状态跳转表 (自动生成)
        // ==========================================
        switch (state) {
{{DFA_SWITCH_CASE}} 
            default:
                break;
        }
        // ==========================================

        if (nextState != -1) {
            // 状态转移成功
            state = nextState;
            advance(); // 真的吃掉字符
            
            // 只有当不是状态0自循环（跳过空格）时，才记录文本
            if (!(state == 0 && nextState == 0)) {
                currentText += c;
            }
        } else {
            // 没路走了，根据当前停留在的状态决定 Token 类型
            
            // 1. 如果还在初始状态 0，说明遇到非法字符
            if (state == 0) {
                 return Token{"ERROR", std::string(1, c), m_line};
            }

            // 2. 根据终态返回 Token (这部分也需要在 Switch Case 里包含，或者单独生成)
            
{{FINAL_STATE_JUDGEMENT}}

            // 兜底
            return Token{"ERROR", "Unrecognized state", m_line};
        }
    }
}
)";

// =========================================================
// 3. Parser 头文件模版 (Parser.h)
// =========================================================
const std::string TEMPLATE_PARSER_H = R"(
#ifndef GENERATED_PARSER_H
#define GENERATED_PARSER_H

#include "lexer.h"
#include <vector>
#include <string>
#include <stack>
#include <iostream>

// --- 核心：语义值结构体 (Semantic Value) ---
// 这个结构体在规约过程中在栈内传递。
// 它必须包含 rules.txt 中可能用到的所有属性。
struct SemanticValue {
    // 1. 基础属性
    std::string text;       // 词法单元的原始文本 (例如 "count", "123")
    int line;               // 行号

    // 2. 语义属性 (SDT / 中间代码生成用)
    std::string code = "";       // 存储生成的代码片段
    std::string var = "";        // 存储临时变量名 (例如 "t1")
    std::string trueLabel = "";  // 布尔表达式为真时的跳转标签 (例如 "L1")
    std::string falseLabel = ""; // 布尔表达式为假时的跳转标签 (例如 "L2")
    std::string beginLabel = ""; // 循环开始标签
    std::string nextLabel = "";  // 语句结束后的连接标签
    
    // 3. 数值属性 (如果是做计算器)
    int val = 0;
};

class Parser {
public:
    Parser(Lexer& lexer);
    
    // 执行解析
    // 返回 true 表示解析成功，false 表示失败
    bool parse();

private:
    Lexer& m_lexer;
    
    // LR 分析的核心栈
    std::stack<int> m_stateStack;           // 状态栈
    std::stack<SemanticValue> m_valueStack; // 值栈 ($1, $2, $$...)

    // 辅助：查 GOTO 表
    // 给定当前状态和非终结符(LHS)，返回下一个状态
    int getGoto(int state, const std::string& lhs);
    
    // 辅助：打印错误信息
    void reportError(const Token& token);
};

#endif // GENERATED_PARSER_H
)";

// =========================================================
// 4. Parser 实现文件模版 (Parser.cpp)
// =========================================================
const std::string TEMPLATE_PARSER_CPP = R"(
#include "parser.h"
#include <sstream>

// =========================================================
//  全局辅助函数 (供 rules.txt 中的语义动作调用)
// =========================================================

static int g_tempCount = 0;
static int g_labelCount = 0;

// 生成临时变量名: t1, t2, ...
std::string newTemp() {
    return "t" + std::to_string(++g_tempCount);
}

// 生成标签名: L1, L2, ...
std::string newLabel() {
    return "L" + std::to_string(++g_labelCount);
}

// 输出中间代码 (这里简单打印到控制台，实际可以写入文件)
void emit(const std::string& code) {
    std::cout << code << std::endl;
}

// =========================================================
//  Parser 类实现
// =========================================================

Parser::Parser(Lexer& lexer) : m_lexer(lexer) {
    // 初始状态入栈 (通常是 0)
    m_stateStack.push(0);
}

void Parser::reportError(const Token& token) {
    std::cerr << "[Syntax Error] Unexpected token '" << token.type 
              << "' (" << token.text << ") at line " << token.line << std::endl;
}

int Parser::getGoto(int state, const std::string& lhs) {
{{GOTO_TABLE_LOGIC}}
    return -1; // error
}

bool Parser::parse() {
    // 读取第一个 Token (Lookahead)
    Token lookahead = m_lexer.nextToken();

    while (true) {
        int state = m_stateStack.top();
        std::string type = lookahead.type;

        // 调试日志 (可选)
        // std::cout << "State: " << currentState << ", Lookahead: " << type << std::endl;

        // ============================================================
        //  ACTION 表逻辑 (由代码生成器填充)
        // ============================================================
        // 分支结构，决定是 Shift, Reduce, Accept 还是 Error

        // CodeEmitter 生成一系列 if-else 语句替换这里
        
{{ACTION_TABLE_LOGIC}}
       
    }
}
)";
