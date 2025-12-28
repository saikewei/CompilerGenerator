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
#include "Lexer.h"

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