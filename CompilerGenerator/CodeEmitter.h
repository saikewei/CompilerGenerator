#pragma once

#include "Types.h"
#include <string>

class CodeEmitter {
public:
    CodeEmitter();

    // 1. 生成词法分析器代码 (lex.cpp / lex.h)
    // 根据 DFA 表，生成 switch-case 跳转代码
    bool emitLexer(const std::string& filename, const DFATable& dfa);

    // 2. 生成语法分析器代码 (parser.cpp / parser.h)
    // 根据 LR 表和产生式，生成栈操作代码和语义动作 switch-case
    bool emitParser(const std::string& filename,
        const ActionTable& actionTbl,
        const GotoTable& gotoTbl,
        const std::vector<ProductionRule>& rules);

    // 辅助：读取用户输入的规则文件 (.txt)，并解析内容填充到 vector<string> 中供 A 和 B 使用
    // 返回值：是否读取成功
    bool parseInputFile(const std::string& filepath,
        std::vector<TokenDefinition>& outTokens,
        std::vector<ProductionRule>& outGrammar);
};
