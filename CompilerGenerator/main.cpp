#pragma once
// main.cpp
#include "LexerGenerator.h"
#include "ParserGenerator.h"
#include "CodeEmitter.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // 默认读取 rules.txt
    std::string filename = "rules.txt";
    if (argc >= 2) {
        filename = argv[1];
    }

    std::cout << "============================================" << std::endl;
    std::cout << "   Compiler Generator (The C++ Team)" << std::endl;
    std::cout << "============================================" << std::endl;

    // ---------------------------------------------------------
    // 阶段 1: 读取与解析 (前端)
    // ---------------------------------------------------------
    std::cout << "[Step 1] Parsing rule file: " << filename << "..." << std::endl;

    CodeEmitter emitter;
    std::vector<TokenDefinition> tokenDefs;
    std::vector<ProductionRule> grammarRules;

    if (!emitter.parseInputFile(filename, tokenDefs, grammarRules)) {
        std::cerr << "[Error] Failed to parse input file. Aborting." << std::endl;
        return 1;
    }

    std::cout << "   -> Found " << tokenDefs.size() << " lexical rules." << std::endl;
    std::cout << "   -> Found " << grammarRules.size() << " grammar rules." << std::endl;

    if (tokenDefs.empty() || grammarRules.empty()) {
        std::cerr << "[Warning] Rules seem empty. Check your input file format." << std::endl;
    }

    // ---------------------------------------------------------
    // 阶段 2: 构建词法分析器
    // ---------------------------------------------------------
    std::cout << "[Step 2] Building Lexer (DFA Construction)..." << std::endl;

    LexerGenerator lexGen;

    // 将解析出的 Token 规则喂给 LexerGenerator
    for (const auto& token : tokenDefs) {
        lexGen.addRule(token.name, token.pattern);
    }

    // 执行核心算法 (Regex -> NFA -> DFA)
    lexGen.build();

    std::cout << "   -> Lexer build complete." << std::endl;

    // ---------------------------------------------------------
    // 阶段 3: 构建语法分析器
    // ---------------------------------------------------------
    std::cout << "[Step 3] Building Parser (LR Table Construction)..." << std::endl;

    ParserGenerator parserGen;

    // 默认将第一条语法规则的左部设为起始符号 (Start Symbol)
    if (!grammarRules.empty()) {
        parserGen.setStartSymbol(grammarRules[0].lhs);
    }

    // 将解析出的语法规则喂给 ParserGenerator
    for (const auto& rule : grammarRules) {
        parserGen.addProduction(rule.lhs, rule.rhs, rule.semanticAction);
    }

    // 执行核心算法 (First/Follow -> Items -> LR Table)
    parserGen.build();

    std::cout << "   -> Parser build complete." << std::endl;

    // ---------------------------------------------------------
    // 阶段 4: 代码生成
    // ---------------------------------------------------------
    std::cout << "[Step 4] Emitting Target C++ Code..." << std::endl;

    // 定义输出文件名
    std::string lexOutFile = "lex.cpp";
    std::string parserOutFile = "parser.cpp";

    // 生成 lex.cpp
    if (!emitter.emitLexer(lexOutFile, lexGen.getDFATable())) {
        std::cerr << "[Error] Failed to generate lexer code." << std::endl;
        return 1;
    }

    // 生成 parser.cpp
    if (!emitter.emitParser(parserOutFile,
        parserGen.getActionTable(),
        parserGen.getGotoTable(),
        parserGen.getRules())) {
        std::cerr << "[Error] Failed to generate parser code." << std::endl;
        return 1;
    }

    std::cout << "============================================" << std::endl;
    std::cout << "   Success! Generated files:" << std::endl;
    std::cout << "   1. " << lexOutFile << std::endl;
    std::cout << "   2. " << parserOutFile << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}
