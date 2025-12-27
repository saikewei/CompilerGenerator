#include "ParserGenerator.h"
#include "Types.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        size_t start = item.find_first_not_of(" \t\r\n");
        size_t end = item.find_last_not_of(" \t\r\n");
        if (start != std::string::npos) {
            result.push_back(item.substr(start, end - start + 1));
        }
    }
    return result;
}

int main() {
    std::cout << "========== LR(1) Parser Generator Test ==========" << std::endl;
    std::cout << std::endl;

    ParserGenerator generator;

    std::string startSymbol;
    std::cout << "Start Symbol: ";
    std::cin >> startSymbol;
    generator.setStartSymbol(startSymbol);
    std::cout << std::endl;

    std::cout << "Number of Productions: ";
    int numProductions;
    std::cin >> numProductions;
    std::cin.ignore();

    std::cout << "Enter productions (format: A -> a b c)" << std::endl;
    std::cout << "Use 'eps' for empty string" << std::endl;
    std::cout << std::endl;

    for (int i = 0; i < numProductions; ++i) {
        std::cout << "Production " << (i + 1) << ": ";
        std::string line;
        std::getline(std::cin, line);

        size_t arrowPos = line.find("->");
        if (arrowPos == std::string::npos) {
            std::cerr << "Error: Missing '->' in production" << std::endl;
            --i;
            continue;
        }

        std::string lhs = line.substr(0, arrowPos);
        std::string rhsStr = line.substr(arrowPos + 2);

        size_t start = lhs.find_first_not_of(" \t\r\n");
        size_t end = lhs.find_last_not_of(" \t\r\n");
        if (start != std::string::npos) {
            lhs = lhs.substr(start, end - start + 1);
        }

        start = rhsStr.find_first_not_of(" \t\r\n");
        end = rhsStr.find_last_not_of(" \t\r\n");
        if (start != std::string::npos) {
            rhsStr = rhsStr.substr(start, end - start + 1);
        }

        std::vector<std::string> rhs = splitString(rhsStr, ' ');

        generator.addProduction(lhs, rhs, "");
    }

    std::cout << std::endl;

    std::cout << "Building LR(1) parsing table..." << std::endl;
    generator.build();
    std::cout << "Build completed!" << std::endl;
    std::cout << std::endl;

    const ActionTable& actionTable = generator.getActionTable();
    std::cout << "========== ACTION Table ==========" << std::endl;
    std::cout << std::endl;

    if (actionTable.empty()) {
        std::cout << "(empty)" << std::endl;
    }
    else {
        std::cout << "State\tSymbol\t\tAction" << std::endl;
        std::cout << "-------------------------------------------" << std::endl;

        for (const auto& entry : actionTable) {
            int state = entry.first.first;
            const std::string& symbol = entry.first.second;
            const LRAction& action = entry.second;

            std::cout << state << "\t" << symbol << "\t\t";

            switch (action.type) {
            case ACTION_SHIFT:
                std::cout << "shift " << action.target;
                break;
            case ACTION_REDUCE:
                std::cout << "reduce R" << action.target;
                break;
            case ACTION_ACCEPT:
                std::cout << "accept";
                break;
            case ACTION_ERROR:
                std::cout << "error";
                break;
            }
            std::cout << std::endl;
        }
    }

    std::cout << std::endl;

    const GotoTable& gotoTable = generator.getGotoTable();
    std::cout << "========== GOTO Table ==========" << std::endl;
    std::cout << std::endl;

    if (gotoTable.empty()) {
        std::cout << "(empty)" << std::endl;
    }
    else {
        std::cout << "State\tNonterminal\tGoto" << std::endl;
        std::cout << "-------------------------------------------" << std::endl;

        for (const auto& entry : gotoTable) {
            int state = entry.first.first;
            const std::string& nonterminal = entry.first.second;
            int targetState = entry.second;

            std::cout << state << "\t" << nonterminal << "\t\t" << targetState << std::endl;
        }
    }

    std::cout << std::endl;

    const std::vector<ProductionRule>& rules = generator.getRules();
    std::cout << "========== Productions ==========" << std::endl;
    std::cout << std::endl;
    std::cout << "Total: " << rules.size() << " productions" << std::endl;

    for (const auto& rule : rules) {
        std::cout << "R" << rule.id << ": " << rule.lhs << " -> ";
        for (size_t i = 0; i < rule.rhs.size(); ++i) {
            std::cout << rule.rhs[i];
            if (i + 1 < rule.rhs.size()) std::cout << " ";
        }
        std::cout << std::endl;
    }

    std::cout << std::endl;
    std::cout << "========== Test Complete ==========" << std::endl;

    return 0;
}
