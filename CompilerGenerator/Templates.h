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
    // 外层循环：用于处理 SKIP 类型的 Token
    // 如果匹配到了 SKIP，循环会继续，直到匹配到非 SKIP 或 EOF
    while (true) {
        
        // 使用 Lambda 封装一次 DFA 贪婪匹配过程
        auto matchOneToken = [&]() -> Token {
            // EOF 检查
            if (m_pos >= m_source.length()) {
                return Token{"#", "", m_line};
            }

            int state = 0;       // 初始状态
            std::string currentText;
            
            // 贪婪匹配循环
            while (true) {
                char c = peek(); 
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
                    advance(); // 吃掉字符
                    currentText += c; // 记录文本
                } else {
                    // 没路走了，根据当前停留在的状态决定 Token 类型
                    
                    // 1. 如果还在初始状态 0，说明遇到的第一个字符就是非法的
                    if (state == 0) {
                         // 移动指针避免死循环，并返回错误
                         char errC = advance(); 
                         return Token{"ERROR", std::string(1, errC), m_line};
                    }

                    // 2. 根据终态返回 Token 
                    // (生成的代码包含 return 语句，这里会退出 Lambda)
{{FINAL_STATE_JUDGEMENT}}

                    // 兜底：虽然走了很多步，但停在了一个非终态上
                    return Token{"ERROR", "Unrecognized state: " + currentText, m_line};
                }
            }
        };

        // === 执行匹配 ===
        Token token = matchOneToken();

        // === 关键逻辑 ===
        // 如果 Token 类型是 SKIP (在规则中定义的忽略项)
        // 则不返回给 Parser，而是重置状态继续循环，寻找下一个 Token
        if (token.type == "SKIP") {
            continue;
        }

        // 如果是普通 Token、EOF 或 ERROR，则返回
        return token;
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
#include <algorithm> // 用于合并列表

// --- 核心：语义值结构体 (Semantic Value) ---
struct SemanticValue {
    std::string text;       
    int line;               

    // SDT 属性
    std::string code = "";       
    std::string var = "";        
    
    // 回填 (Backpatching) 专用属性
    std::vector<int> trueList;   
    std::vector<int> falseList;  
    std::vector<int> nextList;   
    
    int quad = 0; // M 标记用
    int val = 0;

    SemanticValue(std::string t = "", int l = 0) : text(t), line(l) {}
};

class Parser {
public:
    Parser(Lexer& lexer);
    
    // 执行解析
    bool parse();

    // 打印最终生成的代码
    void printGeneratedCode() const;

    // 获取生成的代码缓冲区 (如果外部需要)
    const std::vector<std::string>& getCodeBuffer() const { return m_codeBuffer; }

private:
    Lexer& m_lexer;
    std::stack<int> m_stateStack;           
    std::stack<SemanticValue> m_valueStack; 

    // --- 中间代码生成器状态 (原全局变量) ---
    std::vector<std::string> m_codeBuffer; // 代码缓冲区
    int m_tempCount;                       // 临时变量计数
    int m_labelCount;                      // 标签计数

    // --- 辅助函数：查表与报错 ---
    int getGoto(int state, const std::string& lhs);
    void reportError(const Token& token);

    // --- 辅助函数：中间代码生成 (供语义动作调用) ---
    int nextquad() const;
    std::string newTemp();
    std::string newLabel();
    void emit(const std::string& code);
    
    // --- 辅助函数：回填逻辑 ---
    std::vector<int> makelist(int index);
    std::vector<int> merge(const std::vector<int>& list1, const std::vector<int>& list2);
    void backpatch(const std::vector<int>& list, int targetQuad);
    void backpatch(const std::vector<int>& list, const std::string& targetLabel);
};

#endif // GENERATED_PARSER_H
)";

// =========================================================
// 4. Parser 实现文件模版 (Parser.cpp)
// =========================================================
const std::string TEMPLATE_PARSER_CPP = R"(
#include "parser.h"
#include <sstream>
#include <iomanip>

// =========================================================
//  Parser 类实现
// =========================================================

Parser::Parser(Lexer& lexer) 
    : m_lexer(lexer), m_tempCount(0), m_labelCount(0) 
{
    // 初始状态入栈
    m_stateStack.push(0);
    // 缓冲区自动初始化为空
}

// ---------------------------------------------------------
//  中间代码生成辅助函数 (成员函数实现)
// ---------------------------------------------------------

int Parser::nextquad() const {
    return (int)m_codeBuffer.size();
}

std::string Parser::newTemp() {
    return "t" + std::to_string(++m_tempCount);
}

std::string Parser::newLabel() {
    return "L" + std::to_string(++m_labelCount);
}

void Parser::emit(const std::string& code) {
    m_codeBuffer.push_back(code);
}

std::vector<int> Parser::makelist(int index) {
    std::vector<int> list;
    list.push_back(index);
    return list;
}

std::vector<int> Parser::merge(const std::vector<int>& list1, const std::vector<int>& list2) {
    std::vector<int> merged = list1;
    merged.insert(merged.end(), list2.begin(), list2.end());
    return merged;
}

void Parser::backpatch(const std::vector<int>& list, int targetQuad) {
    std::string targetStr = std::to_string(targetQuad);
    for (int index : list) {
        if (index >= 0 && index < (int)m_codeBuffer.size()) {
            // 在指令末尾追加目标地址
            m_codeBuffer[index] += " " + targetStr; 
        }
    }
}

void Parser::backpatch(const std::vector<int>& list, const std::string& targetLabel) {
    for (int index : list) {
        if (index >= 0 && index < (int)m_codeBuffer.size()) {
            m_codeBuffer[index] += " " + targetLabel;
        }
    }
}

void Parser::printGeneratedCode() const {
    std::cout << "\n=== Intermediate Code Generation ===\n";
    for (size_t i = 0; i < m_codeBuffer.size(); ++i) {
        std::cout << std::setw(3) << i << ": " << m_codeBuffer[i] << std::endl;
    }
    std::cout << "====================================\n";
}

// ---------------------------------------------------------
//  核心解析逻辑
// ---------------------------------------------------------

void Parser::reportError(const Token& token) {
    std::cerr << "[Syntax Error] Unexpected token '" << token.type 
              << "' (" << token.text << ") at line " << token.line << std::endl;
}

int Parser::getGoto(int state, const std::string& lhs) {
{{GOTO_TABLE_LOGIC}}
    return -1; 
}

bool Parser::parse() {
    Token lookahead = m_lexer.nextToken();

    while (true) {
        int state = m_stateStack.top();
        std::string type = lookahead.type;

        // ============================================================
        //  ACTION 表逻辑
        // ============================================================
        
{{ACTION_TABLE_LOGIC}}
       
    }
}
)";