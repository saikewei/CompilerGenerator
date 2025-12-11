#pragma once

#include "ParserGenerator.h"
#include <iostream>

// 构造函数
ParserGenerator::ParserGenerator() {
}

void ParserGenerator::setStartSymbol(const std::string& startSymbol) {
    this->startSymbol = startSymbol;
    std::cout << "[ParserGen] Start symbol set to: " << startSymbol << std::endl;
}

void ParserGenerator::addProduction(const std::string& lhs, const std::vector<std::string>& rhs, const std::string& actionCode) {
    // [TODO: 队友B在此处实现]
    ProductionRule rule;
    rule.id = (int)this->productions.size(); // 简单的自增ID
    rule.lhs = lhs;
    rule.rhs = rhs;
    rule.semanticAction = actionCode;

    this->productions.push_back(rule);

    std::cout << "[ParserGen] Added production: " << lhs << " -> ... (" << actionCode.size() << " bytes action)" << std::endl;
}

void ParserGenerator::build() {
    // [TODO: 队友B在此处实现 First/Follow集 和 LR表构建]
    std::cout << "[ParserGen] Building LR Table... (Not Implemented Yet)" << std::endl;

    // 造一些假数据防止空指针
    // 假设状态0遇到 "NUM" 移进到状态1
    LRAction dummyAction;
    dummyAction.type = ACTION_SHIFT;
    dummyAction.target = 1;
    this->actionTable[{0, "NUM"}] = dummyAction;
}

const ActionTable& ParserGenerator::getActionTable() const {
    return this->actionTable;
}

const GotoTable& ParserGenerator::getGotoTable() const {
    return this->gotoTable;
}

const std::vector<ProductionRule>& ParserGenerator::getRules() const {
    return this->productions;
}