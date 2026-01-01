# CompilerGenerator

A simple compiler generator that generates compilers from grammar rules.

## How to Run

### Generate Compiler

This is a Visual Studio project. Open and run it with **Visual Studio 2022**.

- **Rules file**: `CompilerGenerator/rules.txt`
- **Generated compiler code**: `CompilerGenerator/output/`
- **Test code**: `CompilerGenerator/output/code.txt`

### Run Generated Compiler

To avoid creating repetitive Visual Studio projects, the generated compiler code is designed to be compiled and run in a **Linux environment with GCC**:

```bash
cd CompilerGenerator/output
chmod +x run.sh
./run.sh
```

The `run.sh` script will compile the generated compiler using GCC and execute it against the test code.
