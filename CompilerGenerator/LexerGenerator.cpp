#pragma once

#include "LexerGenerator.h"
#include <iostream>

// 构造函数
LexerGenerator::LexerGenerator() {
    // 初始化逻辑（如果有）
}

void LexerGenerator::addRule(const std::string& tokenName, const std::string& regex) {
    // [TODO: 队友A在此处实现] 
    // 暂时把规则存起来，方便调试打印
    TokenDefinition def = { tokenName, regex };
    this->rules.push_back(def);

    std::cout << "[LexerGen] Added rule: " << tokenName << " -> " << regex << std::endl;
}

void LexerGenerator::build() {
    // [TODO: 队友A在此处实现 Thompson算法 和 子集构造法]
    std::cout << "[LexerGen] Building DFA... (Not Implemented Yet)" << std::endl;

    // 为了防止空跑报错，我们可以造一个假数据
    // 假设不管输入什么，都停在状态 0
    DFARow dummyRow;
    dummyRow.stateID = 0;
    dummyRow.isFinal = true;
    dummyRow.tokenName = "DUMMY";
    this->dfaTable.push_back(dummyRow);
}

const DFATable& LexerGenerator::getDFATable() const {
    return this->dfaTable;
}