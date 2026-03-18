#ifndef SEMANTIC_H
#define SEMANTIC_H

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <stdexcept>
#include "../parser/ast.h"

using namespace std;

typedef struct {
    enum class Kind { DATA, VAR, CONST, PROC } kind;
    string name;
    string type;
    bool is_array = false;
    int array_size = 0;
} symbol_t;

class SymbolTable {
public:
    void push_scope();
    void pop_scope();
    void declare(const string &name, symbol_t sym);
    symbol_t *lookup(const string &name);
    bool has_active_scope();

private:
    vector<map<string, symbol_t>> scopes;
};

class SemanticAnalyzer {
public:
    void analyze(Program &program);
    SymbolTable &get_globals();

private:
    SymbolTable globals;
    SymbolTable locals;
    vector<string> errors;
    bool in_loop = false;
    string curr_proc;

    void collect_declarations(Program &program);
    void collect_sections(SectionDecl &section);
    void check_bodies(Program &program);
    void check_proc(ProcDecl &proc);
    void check_statement(ASTNode &node);
    void check_when(WhenBlock &node);
    void check_loop(LoopBlock &node);
    void check_frame(FrameBlock &node);
    void check_assign(AssignStmt &node);
    string resolve_type(ASTNode &expr);
    void report(const string &msg);
};

#endif