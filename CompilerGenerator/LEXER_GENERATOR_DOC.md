# 词法分析生成器 (LexerGenerator) 实现文档

## 概述

本文档描述了 Member A（词法分析生成专家）的实现内容。该模块负责将正则表达式转换为确定有限自动机（DFA），用于自动生成词法分析器。

## 核心功能

1. **正则表达式预处理** - 处理转义字符和字符类
2. **Thompson 算法** - 将正则表达式转换为非确定有限自动机（NFA）
3. **子集构造法** - 将 NFA 转换为 DFA
4. **Hopcroft 算法** - DFA 最小化
5. **DFA 表生成** - 生成供代码生成器使用的 DFA 转换表

## 数据结构

### NFAState（NFA 状态）

```cpp
struct NFAState {
    int id;                                    // 状态ID
    bool isFinal;                              // 是否为终态
    std::string tokenName;                     // 终态对应的Token名称
    std::map<char, std::set<int>> transitions; // 字符转换：字符 -> 目标状态集合
    std::set<int> epsilonTransitions;          // epsilon转换：目标状态集合
};
```

### NFA（非确定有限自动机）

```cpp
struct NFA {
    int startState;                    // 起始状态ID
    int endState;                      // 终态ID
    std::map<int, NFAState> states;   // 状态集合
};
```

### DFASubset（DFA 子集状态）

```cpp
struct DFASubset {
    std::set<int> nfaStates;           // 包含的NFA状态集合
    bool isFinal;                      // 是否为终态
    std::string tokenName;             // Token名称
    int dfaStateID;                    // DFA状态ID
    std::map<char, int> transitions;  // 字符转换：字符 -> 目标DFA状态ID
};
```

## API 接口

### LexerGenerator 类

#### 构造函数

```cpp
LexerGenerator();
```

初始化词法分析生成器。

#### 添加规则

```cpp
void addRule(const std::string& tokenName, const std::string& regex);
```

**参数：**
- `tokenName`: Token 名称（如 "NUM", "ID"）
- `regex`: 正则表达式（如 "[0-9]+", "[a-zA-Z_]+"）

**示例：**
```cpp
LexerGenerator lexGen;
lexGen.addRule("NUM", "[0-9]+");
lexGen.addRule("ID", "[a-zA-Z_][a-zA-Z0-9_]*");
```

#### 构建 DFA

```cpp
void build();
```

执行完整的构建流程：
1. 为每条规则构建 NFA
2. 合并所有 NFA
3. 使用子集构造法转换为 DFA
4. 使用 Hopcroft 算法最小化 DFA
5. 生成 DFATable

#### 获取 DFA 表

```cpp
const DFATable& getDFATable() const;
```

返回生成的 DFA 转换表，供代码生成器使用。

## 核心算法详解

### 1. 正则表达式预处理 (`preprocessRegex`)

**功能：** 处理转义字符和字符类

**支持的特性：**
- 转义字符：`\n` (换行), `\t` (制表符), `\r` (回车)`
- 字符类：`[0-9]` → `(0|1|2|3|4|5|6|7|8|9)`
- 字符范围：`[a-z]`, `[A-Z]` 等

**示例：**
```
输入: "[0-9]+"
输出: "(0|1|2|3|4|5|6|7|8|9)+"
```

### 2. 正则表达式转后缀表达式 (`regexToPostfix`)

**功能：** 将中缀正则表达式转换为后缀表达式（逆波兰表示法）

**支持的运算符：**
- `*` - Kleene 闭包（0次或多次）
- `+` - 正闭包（1次或多次）
- `?` - 可选（0次或1次）
- `|` - 选择（或）
- `.` - 连接（隐式，自动添加）

**运算符优先级：**
1. `*`, `+`, `?` (优先级 3)
2. `.` (连接，优先级 2)
3. `|` (选择，优先级 1)

**示例：**
```
输入: "a|b*c"
输出: "abc*|"
```

### 3. Thompson 算法 (`regexToNFA`)

**功能：** 将正则表达式转换为 NFA

**算法步骤：**
1. 将正则表达式转换为后缀表达式
2. 使用栈构建 NFA：
   - **单个字符**：创建两个状态，用字符边连接
   - **连接 (.)**：将两个 NFA 的终态连接到下一个 NFA 的起始状态
   - **选择 (|)**：创建新的起始和终态，用 epsilon 边连接到两个 NFA
   - **Kleene 闭包 (*)**：添加 epsilon 边形成循环
   - **正闭包 (+)**：至少匹配一次，然后可以循环
   - **可选 (?)**：允许 0 次或 1 次匹配

**示例：**
```
正则表达式: "a|b"
NFA结构:
    [start] --ε--> [NFA_a_start]
    [start] --ε--> [NFA_b_start]
    [NFA_a_end] --ε--> [end]
    [NFA_b_end] --ε--> [end]
```

### 4. 子集构造法 (`nfaToDFA`)

**功能：** 将 NFA 转换为 DFA

**算法步骤：**
1. **初始化**：计算起始状态的 epsilon 闭包
2. **主循环**：
   - 对每个 DFA 状态和每个输入字符：
     - 计算 `move(T, c)` - 状态集合在字符 c 下的转换
     - 计算 epsilon 闭包
     - 如果是新状态，加入工作队列
3. **构建转换表**：记录每个 DFA 状态的转换

**关键函数：**

#### epsilonClosure
计算状态集合的 epsilon 闭包（通过 epsilon 边可达的所有状态）

```cpp
std::set<int> epsilonClosure(const NFA& nfa, const std::set<int>& states);
```

#### move
计算状态集合在某个字符下的转换

```cpp
std::set<int> move(const NFA& nfa, const std::set<int>& states, char c);
```

### 5. Hopcroft 算法 (`minimizeDFA`)

**功能：** 最小化 DFA，减少状态数量

**算法步骤：**
1. **初始划分**：将状态分为接受状态和非接受状态
2. **迭代细化**：
   - 对每个划分，检查状态在相同输入下的转换是否指向同一划分
   - 如果不同，将状态分离到新的划分
3. **构建最小化 DFA**：每个划分成为一个新状态

**优势：**
- 减少状态数量
- 提高词法分析效率
- 减少生成代码的大小

## 使用示例

### 基本使用

```cpp
#include "LexerGenerator.h"
#include <iostream>

int main() {
    LexerGenerator lexGen;
    
    // 添加词法规则
    lexGen.addRule("NUM", "[0-9]+");
    lexGen.addRule("ID", "[a-zA-Z_][a-zA-Z0-9_]*");
    lexGen.addRule("PLUS", "\\+");
    lexGen.addRule("MINUS", "\\-");
    
    // 构建 DFA
    lexGen.build();
    
    // 获取 DFA 表
    const DFATable& dfaTable = lexGen.getDFATable();
    
    // 打印 DFA 信息
    std::cout << "DFA has " << dfaTable.size() << " states." << std::endl;
    
    return 0;
}
```

### 与主程序集成

在 `main.cpp` 中的使用：

```cpp
LexerGenerator lexGen;

// 从规则文件读取词法规则
for (const auto& token : tokenDefs) {
    lexGen.addRule(token.name, token.pattern);
}

// 执行核心算法
lexGen.build();

// 生成词法分析器代码
emitter.emitLexer(lexGen.getDFATable());
```

## 支持的正则表达式特性

### 基本操作符

| 操作符 | 说明 | 示例 |
|--------|------|------|
| `*` | 0次或多次 | `a*` 匹配 "", "a", "aa", ... |
| `+` | 1次或多次 | `a+` 匹配 "a", "aa", ... |
| `?` | 0次或1次 | `a?` 匹配 "", "a" |
| `|` | 选择 | `a|b` 匹配 "a" 或 "b" |
| `.` | 连接（隐式） | `ab` 匹配 "ab" |

### 字符类

- `[0-9]` - 数字 0-9
- `[a-z]` - 小写字母 a-z
- `[A-Z]` - 大写字母 A-Z
- `[a-zA-Z]` - 所有字母

### 转义字符

- `\n` - 换行符
- `\t` - 制表符
- `\r` - 回车符
- `\\` - 反斜杠
- `\+`, `\-`, `\*` 等 - 转义特殊字符

## 算法复杂度

| 算法 | 时间复杂度 | 空间复杂度 |
|------|-----------|-----------|
| Thompson 算法 | O(n) | O(n) |
| 子集构造法 | O(2^n × m) | O(2^n) |
| Hopcroft 算法 | O(n log n) | O(n) |

其中：
- n = NFA/DFA 状态数
- m = 输入字符集大小

## 输出格式

生成的 `DFATable` 包含以下信息：

```cpp
struct DFARow {
    int stateID;                    // 状态ID
    bool isFinal;                   // 是否为终态
    std::string tokenName;          // Token名称（如果是终态）
    std::map<char, int> transitions; // 字符转换表
};
```

## 注意事项

1. **规则优先级**：规则按添加顺序确定优先级，先添加的规则优先级更高
2. **字符类限制**：当前实现支持简单的字符范围（如 `[0-9]`），复杂字符类可能需要扩展
3. **转义处理**：某些特殊字符需要转义（如 `\+`, `\-`, `\*`）
4. **内存使用**：对于大型正则表达式，DFA 状态数可能很大，需要注意内存使用

## 调试信息

构建过程中会输出详细的调试信息：

```
[LexerGen] Added rule: NUM -> [0-9]+
[LexerGen] Building DFA from 5 rules...
[LexerGen] Converting regex to NFA: NUM -> [0-9]+
[LexerGen] Merging NFAs...
[LexerGen] Converting NFA to DFA (Subset Construction)...
[LexerGen] DFA has 15 states.
[LexerGen] Minimizing DFA (Hopcroft Algorithm)...
[LexerGen] Minimized DFA has 8 states.
[LexerGen] Build complete!
```

## 扩展建议

1. **支持更多正则特性**：
   - 字符类取反 `[^0-9]`
   - 预定义字符类 `\d`, `\w`, `\s`
   - 锚点 `^`, `$`

2. **性能优化**：
   - 缓存 epsilon 闭包计算结果
   - 优化字符类展开

3. **错误处理**：
   - 检测无效的正则表达式
   - 提供更详细的错误信息

## 参考文献

1. Aho, A. V., Lam, M. S., Sethi, R., & Ullman, J. D. (2006). *Compilers: Principles, Techniques, and Tools* (2nd ed.). Pearson Education.
2. Thompson, K. (1968). Programming techniques: Regular expression search algorithm. *Communications of the ACM*, 11(6), 419-422.
3. Hopcroft, J. (1971). An n log n algorithm for minimizing states in a finite automaton. *Theory of machines and computations*, 189-196.

## 作者

Member A - 词法分析生成专家

## 更新日志

- **2024-01-01**: 初始实现
  - 实现 Thompson 算法
  - 实现子集构造法
  - 实现 Hopcroft DFA 最小化算法
  - 完成正则表达式预处理和后缀转换

