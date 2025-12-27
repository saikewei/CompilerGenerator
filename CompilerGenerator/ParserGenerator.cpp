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

    //// 造一些假数据防止空指针
    //// 假设状态0遇到 "NUM" 移进到状态1
    //LRAction dummyAction;
    //dummyAction.type = ACTION_SHIFT;
    //dummyAction.target = 1;
    //this->actionTable[{0, "NUM"}] = dummyAction;
	if (productions.empty()) return;

	//先区分终结符与非终结符
	std::unordered_set<std::string> nonterminals, terminals;
	sortSymbols(nonterminals, terminals, productions);

	//先计算first集合
	std::unordered_map<std::string, std::unordered_set<std::string>>first = 
		computeFirstSets(nonterminals, terminals,productions);

	//增广
	auto augmentedProductions = buildAugmentedProductions(startSymbol,productions);

	//构建LR(1)项目集组
	auto itemSets = buildLR1ItemSets(augmentedProductions, first);

	//构建LR(1)预测分析表
	buildLR1ParsingTable(nonterminals, terminals,itemSets, augmentedProductions, actionTable, gotoTable);
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


void ParserGenerator::sortSymbols(std::unordered_set<std::string>& nonterminals,
	std::unordered_set<std::string>& terminals,
	const std::vector<ProductionRule>& productions) {
	// 收集所有左部出现的非终结符
	for (const auto& p : productions) {
		nonterminals.insert(p.lhs);
	}

	// 扫描右部：不在非终结符集合且不等于 EPS 的视为终结符
	for (const auto& p : productions) {
		for (const auto& sym : p.rhs) {
			if (!nonterminals.count(sym) && sym != EPS) {
				terminals.insert(sym);
			}
		}
	}

	terminals.insert(END_MARKER);  // 加入结束标记
}


std::unordered_map<std::string, std::unordered_set<std::string>> ParserGenerator::computeFirstSets
	(std::unordered_set<std::string>& nonterminals,
		std::unordered_set<std::string>& terminals, 
		const std::vector<ProductionRule>& productions){
		
		// 初始化 FIRST 集
		std::unordered_map<std::string, std::unordered_set<std::string>> first;
		for (const auto& t : terminals) {
			first[t].insert(t);
		}
		for (const auto& nt : nonterminals) {
			first[nt];  // 确保键存在
		}

		bool changed = true;
		while (changed) {
			changed = false;
			for (const auto& p : productions) {
				const std::string& A = p.lhs;
				const auto& alpha = p.rhs;

				bool allNullable = true;
				for (size_t i = 0; i < alpha.size(); ++i) {
					const std::string& Xi = alpha[i];

					// 将 FIRST(Xi) 去掉 EPS 后并入 FIRST(A)
					for (const auto& sym : first[Xi]) {
						if (sym == EPS) continue;
						auto insertRes = first[A].insert(sym);
						changed = changed || insertRes.second;
					}

					// 若 Xi 不能推出 EPS，则停止继续向右查看
					if (!first[Xi].count(EPS)) {
						allNullable = false;
						break;
					}
				}

				// 若右部所有符号都可推出 EPS，则 EPS 属于 FIRST(A)
				if (allNullable) {
					auto insertRes = first[A].insert(EPS);
					changed = changed || insertRes.second;
				}
			}
		}

		return first;
}



std::unordered_set<std::string> ParserGenerator::computeFirstOfString(
	const std::vector<std::string>& symbols,
	const std::unordered_map<std::string, std::unordered_set<std::string>>& first) {

	std::unordered_set<std::string> result;

	// 先过滤掉空字符串，构建有效符号列表
	std::vector<std::string> validSymbols;
	for (const auto& sym : symbols) {
		if (!sym.empty()) {
			validSymbols.push_back(sym);
		}
	}

	if (validSymbols.empty()) {
		result.insert(EPS);
		return result;
	}

	for (const auto& sym : validSymbols) {
		// 如果符号在 FIRST 集中（非终结符或已知终结符）
		if (first.find(sym) != first.end()) {
			// 将 FIRST(sym) 去掉 EPS 后加入结果
			for (const auto& t : first.at(sym)) {
				if (t != EPS && !t.empty()) {
					result.insert(t);
				}
			}

			// 如果当前符号不能推导出 EPS，停止
			if (first.at(sym).count(EPS) == 0) {
				return result;
			}
		}
		else {
			// 符号不在 FIRST 集中，视为终结符（如 $ 等特殊符号）
			// 终结符的 FIRST 集就是它自己
			result.insert(sym);
			return result;
		}
	}

	// 所有符号都能推导出 EPS，将 EPS 加入结果
	result.insert(EPS);
	return result;
}


std::set<LR1Item> ParserGenerator::closure(
	const std::set<LR1Item>& items,
	const std::vector<ProductionRule>& productions,
	const std::unordered_map<std::string, std::unordered_set<std::string>>& first) {

	std::set<LR1Item> result = items;
	bool changed = true;

	while (changed) {
		changed = false;
		std::set<LR1Item> toAdd;

		for (const auto& item : result) {
			const auto& prod = productions[item.prodId];

			// 如果点在最右边，跳过
			if (item.dotPos >= prod.rhs.size()) continue;

			// 获取点后面的第一个符号
			const std::string& nextSym = prod.rhs[item.dotPos];

			// 只对非终结符进行扩展（需要检查是否是某个产生式的左部）
			bool isNonterminal = false;
			for (const auto& p : productions) {
				if (p.lhs == nextSym) {
					isNonterminal = true;
					break;
				}
			}

			if (!isNonterminal) continue;

			// 计算 FIRST(βa)，其中 β 是点后面除第一个符号外的部分，a 是向前看符号
			std::vector<std::string> betaA;
			for (size_t i = item.dotPos + 1; i < prod.rhs.size(); ++i) {
				betaA.push_back(prod.rhs[i]);
			}

			// 确保向前看符号非空
			std::string origLookahead = item.lookahead.empty() ? END_MARKER : item.lookahead;
			betaA.push_back(origLookahead);

			std::unordered_set<std::string> firstBetaA = computeFirstOfString(betaA, first);

			// 如果 FIRST(βa) 为空或只有 EPS，则使用原始向前看符号
			std::unordered_set<std::string> lookaheads;
			if (firstBetaA.empty() || (firstBetaA.size() == 1 && firstBetaA.count(EPS))) {
				lookaheads.insert(origLookahead);
			}
			else {
				// 过滤掉 EPS，保留其他符号
				for (const auto& la : firstBetaA) {
					if (la != EPS && !la.empty()) {
						lookaheads.insert(la);
					}
				}
				// 如果过滤后为空（全是EPS），使用原始向前看符号
				if (lookaheads.empty()) {
					lookaheads.insert(origLookahead);
				}
			}

			// 对于 nextSym 的每个产生式，添加项目 [B → ·γ, b]
			for (const auto& p : productions) {
				if (p.lhs != nextSym) continue;

				for (const auto& la : lookaheads) {
					LR1Item newItem;
					newItem.prodId = p.id;
					newItem.dotPos = 0;
					newItem.lookahead = la.empty() ? END_MARKER : la;

					if (result.find(newItem) == result.end()) {
						toAdd.insert(newItem);
						changed = true;
					}
				}
			}
		}

		result.insert(toAdd.begin(), toAdd.end());
	}

	return result;
}



std::set<LR1Item> ParserGenerator::gotoSet(
	const std::set<LR1Item>& items,
	const std::string& symbol,
	const std::vector<ProductionRule>& productions,
	const std::unordered_map<std::string, std::unordered_set<std::string>>& first) {

	std::set<LR1Item> J;

	for (const auto& item : items) {
		const auto& prod = productions[item.prodId];

		// 如果点不在最右边，且点后面的符号是目标符号
		if (item.dotPos < prod.rhs.size() && prod.rhs[item.dotPos] == symbol) {
			LR1Item newItem = item;
			newItem.dotPos++;
			J.insert(newItem);
		}
	}

	if (J.empty()) return J;

	return closure(J, productions, first);
}


std::vector<ProductionRule> ParserGenerator::buildAugmentedProductions
	(const std::string& startSymbol, const std::vector<ProductionRule>& productions) {
	// 准备增广产生式：在调用 buildLR1ItemSets 之前添加
	std::vector<ProductionRule> augmentedProductions;
	ProductionRule augProd;
	augProd.id = 0;
	augProd.lhs = startSymbol + "'";
	augProd.rhs.push_back(startSymbol);
	augmentedProductions.push_back(augProd);

	// 添加原有产生式，并重新编号
	for (const auto& p : productions) {
		ProductionRule newP = p;
		newP.id = augmentedProductions.size();
		augmentedProductions.push_back(newP);
	}

	return augmentedProductions;
}


std::vector<LR1ItemSet> ParserGenerator::buildLR1ItemSets(
	const std::vector<ProductionRule>& productions,
	const std::unordered_map<std::string, std::unordered_set<std::string>>& first) {

	std::vector<LR1ItemSet> itemSets;
	std::map<std::set<LR1Item>, int> itemSetMap;  // 用于快速查找已存在的项目集

	// 创建增广文法的起始项目：S' → ·S, $
	// 增广产生式应该在 productions[0]
	LR1Item startItem;
	startItem.prodId = 0;  // 增广产生式在索引0
	startItem.dotPos = 0;
	startItem.lookahead = END_MARKER;

	std::set<LR1Item> startItems;
	startItems.insert(startItem);

	// 计算初始项目集的闭包
	std::set<LR1Item> I0 = closure(startItems, productions, first);

	LR1ItemSet startSet;
	startSet.items = I0;
	startSet.id = 0;
	itemSets.push_back(startSet);
	itemSetMap[I0] = 0;

	// 收集所有符号（终结符和非终结符），过滤掉 EPS 和空字符串
	std::unordered_set<std::string> allSymbols;
	for (const auto& p : productions) {
		if (!p.lhs.empty()) {
			allSymbols.insert(p.lhs);
		}
		for (const auto& sym : p.rhs) {
			if (sym != EPS && !sym.empty()) {
				allSymbols.insert(sym);
			}
		}
	}

	// 工作队列
	std::vector<int> workList;
	workList.push_back(0);
	size_t workIdx = 0;

	while (workIdx < workList.size()) {
		int currentSetId = workList[workIdx++];
		// 不使用引用，避免 vector 扩容时引用失效。每次直接从 itemSets[currentSetId] 获取
		const auto currentSetItemsCopy = itemSets[currentSetId].items;  // 拷贝一份，确保数据稳定
		std::unordered_set<std::string> nextSymbols;
		for (const auto& it : currentSetItemsCopy) {
			const auto& p = productions[it.prodId];
			if (it.dotPos < p.rhs.size()) {
				const auto& sym = p.rhs[it.dotPos];
				if (!sym.empty() && sym != EPS) nextSymbols.insert(sym);
			}
		}

		for (const auto& symbol : nextSymbols) {
			std::set<LR1Item> gotoItems = gotoSet(currentSetItemsCopy, symbol, productions, first);

			if (gotoItems.empty()) continue;

			// 检查是否已存在
			if (itemSetMap.find(gotoItems) == itemSetMap.end()) {
				LR1ItemSet newSet;
				newSet.items = gotoItems;
				newSet.id = itemSets.size();
				itemSets.push_back(newSet);
				itemSetMap[gotoItems] = newSet.id;
				workList.push_back(newSet.id);
			}
		}
	}

	return itemSets;
}



void ParserGenerator::buildLR1ParsingTable(
	std::unordered_set<std::string>& nonterminals,
	std::unordered_set<std::string>& terminals,
	const std::vector<LR1ItemSet>& itemSets,
	const std::vector<ProductionRule>& productions,
	ActionTable& actionTable,
	GotoTable& gotoTable) {

	// 对每个项目集
	for (const auto& itemSet : itemSets) {
		int stateId = itemSet.id;

		for (const auto& item : itemSet.items) {
			const auto& prod = productions[item.prodId];

			// 情形1: [A → α·aβ, b]（点后面是终结符）
			if (item.dotPos < prod.rhs.size()) {
				const std::string& nextSym = prod.rhs[item.dotPos];

				// 检查是否是终结符
				if (terminals.count(nextSym) || nextSym == END_MARKER) {
					// 计算 GOTO(Ii, a)，找到移进的目标状态
					std::set<LR1Item> shiftedItems;
					for (const auto& it : itemSet.items) {
						const auto& p = productions[it.prodId];
						if (it.dotPos < p.rhs.size() && p.rhs[it.dotPos] == nextSym) {
							LR1Item newItem = it;
							newItem.dotPos++;
							shiftedItems.insert(newItem);
						}
					}

					if (!shiftedItems.empty()) {
						// 找到对应的项目集ID
						for (const auto& targetSet : itemSets) {
							std::set<LR1Item> targetClosure = shiftedItems;
							// 简化：直接比较项目集（实际应该用closure来验证，但这里假设已在itemSets中）
							// 更准确的方法是遍历所有项目集找匹配的
							bool found = false;
							// 通过检查是否所有移进的项都在目标集合中来判断
							int matchCount = 0;
							for (const auto& tItem : targetSet.items) {
								for (const auto& sItem : shiftedItems) {
									if (tItem.prodId == sItem.prodId &&
										tItem.dotPos == sItem.dotPos &&
										tItem.lookahead == sItem.lookahead) {
										matchCount++;
										break;
									}
								}
							}

							if (matchCount == shiftedItems.size() && targetSet.items.size() >= shiftedItems.size()) {
								LRAction action;
								action.type = ACTION_SHIFT;
								action.target = targetSet.id;
								actionTable[{stateId, nextSym}] = action;
								found = true;
								break;
							}
						}
					}
				}
			}

			// 情形2: [A → α·, b]（点在最右边，即归约项）
			if (item.dotPos >= prod.rhs.size()) {
				// 如果是增广产生式 S' → S，则为接受
				if (prod.lhs == productions[0].lhs && item.lookahead == END_MARKER) {
					LRAction action;
					action.type = ACTION_ACCEPT;
					action.target = -1;
					actionTable[{stateId, END_MARKER}] = action;
				}
				else {
					// 否则为归约，在所有向前看符号处设置归约
					LRAction action;
					action.type = ACTION_REDUCE;
					action.target = prod.id;
					actionTable[{stateId, item.lookahead}] = action;
				}
			}
		}

		// 构建 GOTO 表：对非终结符计算 GOTO
		for (const auto& nonterminal : nonterminals) {
			if (nonterminal == productions[0].lhs) continue;  // 跳过增广符号

			// 计算 GOTO(Ii, 非终结符)
			std::set<LR1Item> gotoItems;
			for (const auto& item : itemSet.items) {
				const auto& p = productions[item.prodId];
				if (item.dotPos < p.rhs.size() && p.rhs[item.dotPos] == nonterminal) {
					LR1Item newItem = item;
					newItem.dotPos++;
					gotoItems.insert(newItem);
				}
			}

			if (!gotoItems.empty()) {
				// 在 itemSets 中查找匹配的目标项目集
				for (const auto& targetSet : itemSets) {
					int matchCount = 0;
					for (const auto& tItem : targetSet.items) {
						for (const auto& gItem : gotoItems) {
							if (tItem.prodId == gItem.prodId &&
								tItem.dotPos == gItem.dotPos &&
								tItem.lookahead == gItem.lookahead) {
								matchCount++;
								break;
							}
						}
					}

					if (matchCount == gotoItems.size() && targetSet.items.size() >= gotoItems.size()) {
						gotoTable[{stateId, nonterminal}] = targetSet.id;
						break;
					}
				}
			}
		}
	}
}