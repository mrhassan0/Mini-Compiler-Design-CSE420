Mini C Compiler implemented using Flex and Bison as part of a Compiler Design course.  
The project demonstrates core compiler phases including lexical analysis, syntax parsing,  
symbol table and scope management, abstract syntax tree (AST) construction, and  
three-address code (TAC) generation for a subset of the C programming language.

**HOW TO RUN**  
**Requirements:** Linux/Unix environment, Flex, Bison, GCC/G++

1. Open a terminal in the project directory  
2. Make the script executable: `chmod +x script.sh`  
3. Run the compiler: `./script.sh`  

The script automatically generates the lexer and parser, compiles all components,  
and runs the compiler on the provided input file (`input.c`).

**OUTPUT**
- Tokenization and syntax validation  
- Symbol table with hierarchical scopes  
- Abstract Syntax Tree (AST)  
- Three-Address Code (TAC)  

**NOTES**
- Modify `input.c` to test different programs  
- Error messages include line numbers for debugging  
- Designed for academic and learning purposes  
