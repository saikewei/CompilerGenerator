#pragma once

#include "CodeEmitter.h"
#include "Templates.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

const std::string LEXER_FILENAME = "lexer";
const std::string PARSER_FILENAME = "parser";

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

static bool generateFile(const std::string& filepath, const std::string& content) {
	std::ofstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filepath << " for writing." << std::endl;
        return false;
	}
    file << content;
    file.close();
	return true;
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

bool CodeEmitter::emitLexer(const DFATable& dfa) {
    if (!generateFile(
        (outputDir != nullptr ? *outputDir + "/" + LEXER_FILENAME : LEXER_FILENAME) + ".h",
        TEMPLATE_LEXER_H
    )) {
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
    if (!generateFile(
        (outputDir != nullptr ? *outputDir + "/" + LEXER_FILENAME : LEXER_FILENAME) + ".cpp",
        cppContent
    )) {
        std::cerr << "[CodeEmitter] Failed to generate implementation file." << std::endl;
        return false;
    }

    return true;
}

bool CodeEmitter::emitParser(const ActionTable& actionTbl,
    const GotoTable& gotoTbl,
    const std::vector<ProductionRule>& rules) {

    if(!generateFile(
        (outputDir != nullptr ? *outputDir + "/" + PARSER_FILENAME : PARSER_FILENAME) + ".h",
        TEMPLATE_PARSER_H
	)) {
        std::cerr << "[CodeEmitter] Failed to generate parser header file." << std::endl;
		return false;
	}

	std::stringstream ssGoto;
	std::stringstream ssAction;

	// 生成 GOTO 表逻辑
    for (const auto& entry : gotoTbl) {
        int state = entry.first.first;
		std::string nonTerm = entry.first.second;
        int targetState = entry.second;
        ssGoto << "    " << (ssGoto.str().empty() ? "" : "else ") << "if (state == " << state << " && lhs == \"" << nonTerm << "\") "
			<< "return " << targetState << ";\n";
    }

	// 生成 Action 表逻辑
    for (const auto& entry : actionTbl)
    {
		int state = entry.first.first;
        std::string symbol = entry.first.second;
		LRAction action = entry.second;

		ssAction << "        " << (ssAction.str().empty() ? "" : "else ") << "if (state == " << state << " && lookahead.type == \"" << symbol << "\") {\n";
        switch (action.type)
        {
        case ACTION_SHIFT:
            ssAction << "            // Shift to state " << action.target << "\n"
                     << "            m_stateStack.push(" << action.target << ");\n"
                     << "            m_valueStack.push(SemanticValue{lookahead.text, lookahead.line});\n"
                     << "            lookahead = m_lexer.nextToken();\n";
			break;
		case ACTION_REDUCE:
        {
            // 1. 获取对应的产生式规则
            const ProductionRule& rule = rules[action.target];
            int rhsCount = (int)rule.rhs.size();

            ssAction << "            // Reduce Rule " << rule.id << ": " << rule.lhs << " -> ...\n";

            // 2. 生成弹栈代码
            for (int i = rhsCount; i >= 1; --i) {
                ssAction << "            SemanticValue v" << i << " = m_valueStack.top();\n"
                    << "            m_valueStack.pop();\n"
                    << "            m_stateStack.pop();\n";
            }

            // 3. 处理语义动作字符串替换 ($$ -> res, $1 -> v1, etc.)
            std::string processedAction = rule.semanticAction;

            // 替换 $$ 为 res
            processedAction = replaceAll(processedAction, "$$", "res");

            // 替换 $1, $2... 为 v1, v2...
            for (int i = rhsCount; i >= 1; --i) {
                std::string target = "$" + std::to_string(i);
                std::string replacement = "v" + std::to_string(i);
                processedAction = replaceAll(processedAction, target, replacement);
            }

            // 4. 生成执行语义动作的代码
            ssAction << "            SemanticValue res;\n"; // 准备结果变量
            ssAction << "            " << processedAction << "\n"; // 插入用户写的代码

            // 5. 查 GOTO 表并压入新状态
            ssAction << "            int nextState = getGoto(m_stateStack.top(), \"" << rule.lhs << "\");\n"
                << "            m_stateStack.push(nextState);\n"
                << "            m_valueStack.push(res);\n";
        }
            break;
		case ACTION_ACCEPT:
            ssAction << "            // Accept\n"
                     << "            return true;\n";
			break;
        case ACTION_ERROR:
            ssAction << "            // Error\n"
                     << "            reportError(lookahead);\n"
                     << "            return false;\n";
			break;
        default:
            break;
        }
		ssAction << "        }\n";
    }
    ssAction << "        else {\n"
             << "            // Error\n"
             << "            reportError(lookahead);\n"
             << "            return false;\n"
		<< "        }\n";

	// 渲染模版 (Parser.cpp)
	std::string cppContent = TEMPLATE_PARSER_CPP;
	// 替换占位符
	cppContent = replaceAll(cppContent, "{{GOTO_TABLE_LOGIC}}", ssGoto.str());
	cppContent = replaceAll(cppContent, "{{ACTION_TABLE_LOGIC}}", ssAction.str());
	// 写入文件
    if (!generateFile(
        (outputDir != nullptr ? *outputDir + "/" + PARSER_FILENAME : PARSER_FILENAME) + ".cpp",
        cppContent
    )) {
        std::cerr << "[CodeEmitter] Failed to generate parser implementation file." << std::endl;
        return false;
	}

    return true;
}
