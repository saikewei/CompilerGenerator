#pragma once

#include "CodeEmitter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// 辅助：去除首尾空格
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return str;
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// 辅助：判断是否是空白字符
static bool is_space(char c) {
    return std::isspace(static_cast<unsigned char>(c));
}

// ==========================================
// CodeEmitter 类实现
// ==========================================

CodeEmitter::CodeEmitter() {}

bool CodeEmitter::parseInputFile(const std::string& filepath,
    std::vector<TokenDefinition>& outTokens,
    std::vector<ProductionRule>& outGrammar) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << std::endl;
        return false;
    }

    // 1. 读取整个文件
    std::string content((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    // 2. 切分 Lexical 和 Grammar
    std::string delimiter = "%%";
    size_t pos = content.find(delimiter);
    if (pos == std::string::npos) {
        std::cerr << "Error: Missing '%%' separator." << std::endl;
        return false;
    }

    std::string lexSection = content.substr(0, pos);
    std::string grammarSection = content.substr(pos + delimiter.length());

    // ==========================================
    // 3. 解析词法部分 (Lexical Section) - 这里的正则通常是单行的，保持不变即可
    // ==========================================
    std::stringstream ssLex(lexSection);
    std::string line;
    while (std::getline(ssLex, line)) {
        line = trim(line);
        if (line.empty() || line.rfind("//", 0) == 0) continue;

        std::stringstream ssLine(line);
        std::string pattern, name;
        ssLine >> pattern >> name; // 简单的空格分割
        if (!pattern.empty() && !name.empty()) {
            outTokens.push_back({ name, pattern });
        }
    }

    // ==========================================
    // 4. 解析语法部分 (Grammar Section)
    // ==========================================
    // 策略：指针扫描，支持多行大括号匹配

    size_t cursor = 0;
    size_t len = grammarSection.length();
    int ruleIDCounter = 0;

    while (cursor < len) {
        // --- A. 跳过空白和注释 ---
        while (cursor < len && is_space(grammarSection[cursor])) cursor++;
        if (cursor >= len) break;

        // 处理注释 // 
        if (grammarSection.substr(cursor, 2) == "//") {
            while (cursor < len && grammarSection[cursor] != '\n') cursor++;
            continue; // 重新开始循环处理空白
        }

        ProductionRule rule;
        rule.id = ruleIDCounter++;

        // --- B. 读取 LHS (左部) ---
        size_t start = cursor;
        while (cursor < len && !is_space(grammarSection[cursor]) && grammarSection[cursor] != ':') {
            cursor++;
        }
        rule.lhs = grammarSection.substr(start, cursor - start);

        // --- C. 寻找冒号 ':' ---
        while (cursor < len && is_space(grammarSection[cursor])) cursor++;
        if (cursor >= len || grammarSection[cursor] != ':') {
            // 如果没找到冒号，可能是空行或格式错误，跳过
            cursor++;
            continue;
        }
        cursor++; // 跳过 ':'

        // --- D. 读取 RHS (直到遇到 '{' 或换行/分号，但在我们的格式中主要是 '{') ---
        while (cursor < len) {
            // 跳过空白
            while (cursor < len && is_space(grammarSection[cursor])) cursor++;

            // 检查是否遇到 Action 的开始 '{'
            if (cursor < len && grammarSection[cursor] == '{') {
                break; // 停止 RHS 读取，进入 Action 读取
            }

            // 检查是否遇到下一条规则的开始 (这一步比较难判断，通常假设必须有 Action)
            // 简单处理：读取一个单词作为 RHS 符号
            if (cursor < len) {
                size_t tokenStart = cursor;
                while (cursor < len && !is_space(grammarSection[cursor]) && grammarSection[cursor] != '{') {
                    cursor++;
                }
                std::string symbol = grammarSection.substr(tokenStart, cursor - tokenStart);
                if (!symbol.empty()) {
                    rule.rhs.push_back(symbol);
                }
            }
        }

        // --- E. 读取 Semantic Action (多行括号匹配) ---
        if (cursor < len && grammarSection[cursor] == '{') {
            size_t actionStart = cursor;
            int braceDepth = 0;

            // 开始括号匹配循环
            do {
                if (grammarSection[cursor] == '{') braceDepth++;
                else if (grammarSection[cursor] == '}') braceDepth--;
                cursor++;
            } while (cursor < len && braceDepth > 0);

            // 提取完整的动作代码块 (包含换行符)
            rule.semanticAction = grammarSection.substr(actionStart, cursor - actionStart);
        }
        else {
            // 没有 Action，虽然少见但合法
            rule.semanticAction = "";
        }

        outGrammar.push_back(rule);
    }

    return true;
}

// 占位符函数，防止编译报错，后续再实现
bool CodeEmitter::emitLexer(const std::string& filename, const DFATable& dfa) {
    std::cout << "[CodeEmitter] Generating Lexer to " << filename << " (TODO)" << std::endl;
    return true;
}

bool CodeEmitter::emitParser(const std::string& filename,
    const ActionTable& actionTbl,
    const GotoTable& gotoTbl,
    const std::vector<ProductionRule>& rules) {
    std::cout << "[CodeEmitter] Generating Parser to " << filename << " (TODO)" << std::endl;
    return true;
}
