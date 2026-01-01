# CompilerGenerator

A simple compiler generator that generates compilers from grammar rules.

## How to Run

### Generate Compiler

This is a Visual Studio project. Open and run it with **Visual Studio 2022**.

- **Rules file**: `CompilerGenerator/rules.txt`&`CompilerGenerator/rules2.txt`
- **Generated compiler code**: `CompilerGenerator/output/`
- **Test code**: `CompilerGenerator/output/code_r*.txt`(multiple codes)

_If you want to try rules2.txt, you need to change the file name in `main`._

### Run Generated Compiler

To avoid creating repetitive Visual Studio projects, the generated compiler code is designed to be compiled and run in a **Linux environment with GCC**:

```bash
cd CompilerGenerator/output
chmod +x run.sh
./run.sh
```

The `run.sh` script will compile the generated compiler using GCC and execute it against the test code.
If you want to try different code, you need to copy the code into `code.txt`, so that the compiler can read it.
