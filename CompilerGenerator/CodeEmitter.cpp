#pragma once

#include "CodeEmitter.h"
#include "Templates.h"
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

static std::string replaceAll(std::string str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return str;
}

// ==========================================
// CodeEmitter 类实现
// ==========================================

CodeEmitter::CodeEmitter(): outputDir(nullptr) {}

CodeEmitter::CodeEmitter(const std::string& dir)
{
    if (dir.empty()) {
        outputDir = nullptr;
		std::cout << "[CodeEmitter] No output directory specified, using current directory." << std::endl;
        return;
	}

	outputDir = new std::string(dir);
}

CodeEmitter::~CodeEmitter()
{
    if (outputDir != nullptr) {
        delete outputDir;
    }
}

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

bool CodeEmitter::generateHeader(const std::string& headerFilename = "lexer.h") {
	if (headerFilename.empty()) return false;
    std::ofstream hFile(outputDir != nullptr ? *outputDir + "/" + headerFilename : headerFilename);
    if (!hFile.is_open()) return false;
    hFile << TEMPLATE_LEXER_H;
    hFile.close();

    std::cout << "[CodeEmitter] Generated header: " << headerFilename << std::endl;
	return true;
}

bool CodeEmitter::emitLexer(const std::string& filename, const DFATable& dfa) {
    if (!generateHeader()) {
		std::cerr << "[CodeEmitter] Failed to generate header file." << std::endl;
        return false;
    }

    std::stringstream ssSwitch;
    std::stringstream ssFinal;

    for (const auto& row : dfa) {
        // --- 生成 Switch 部分 ---
        ssSwitch << "            case " << row.stateID << ":\n";
        bool first = true;
        for (auto const& pair : row.transitions) {
            char key = pair.first;   // 获取字符
            int target = pair.second; // 获取目标状态
            if (first) { 
                ssSwitch << "                if "; 
                first = false;
            }
            else { 
                ssSwitch << "                else if "; 
            }

            // 处理特殊字符转义
            if (key == '\n') ssSwitch << "(c == '\\n') ";
            else if (key == '\t') ssSwitch << "(c == '\\t') ";
            else ssSwitch << "(c == '" << key << "') ";

            ssSwitch << "nextState = " << target << ";\n";
        }
        ssSwitch << "                break;\n";

        // --- 生成 Final State 判断部分 ---
        if (row.isFinal) {
            ssFinal << "            if (state == " << row.stateID << ") "
                << "return Token{\"" << row.tokenName << "\", currentText, m_line};\n";
        }
    }

    // 渲染模版 (Lexer.cpp)
    std::string cppContent = TEMPLATE_LEXER_CPP;

    // 替换占位符
    cppContent = replaceAll(cppContent, "{{DFA_SWITCH_CASE}}", ssSwitch.str());
    cppContent = replaceAll(cppContent, "{{FINAL_STATE_JUDGEMENT}}", ssFinal.str());

    // 写入文件
    std::ofstream cFile(outputDir != nullptr ? *outputDir + "/" + filename : filename); // filename 是 Lexer.cpp
    if (!cFile) return false;
    cFile << cppContent;
    cFile.close();
    std::cout << "[CodeEmitter] Generated " << filename << std::endl;

    return true;
}

bool CodeEmitter::emitParser(const std::string& filename,
    const ActionTable& actionTbl,
    const GotoTable& gotoTbl,
    const std::vector<ProductionRule>& rules) {
    std::cout << "[CodeEmitter] Generating Parser to " << filename << " (TODO)" << std::endl;
    return true;
}
