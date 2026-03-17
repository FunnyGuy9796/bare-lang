#ifndef CODEGEN_H
#define CODEGEN_H

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include "../parser/ast.h"
#include "../semantic/semantic.h"

using namespace std;

class CodeGen {
public:
    CodeGen(SymbolTable &globals) : globals(globals), label_count(0) {}
    string generate(Program &program);

private:
    SymbolTable &globals;
    SymbolTable locals;
    int label_count;
    stringstream out;

    int type_size(const string &type);
    string size_word(const string &type);

    map<string, int> data_type_sizes;
    map<string, map<string, int>> field_offsets;
    map<string, string> field_types;

    map<string, int> local_offsets;
    int curr_frame_offset;
    
    string make_label(const string &prefix);
    void gen_program(Program &program);
    void gen_section(SectionDecl &section);
    void gen_conv(ConvDecl &decl);
    void gen_proc(ProcDecl &decl);
    void gen_var_bss(VarDecl &decl);
    void gen_const(ConstDecl &decl);
    void gen_statement(ASTNode &node, const string &break_label = "");
    void gen_frame(FrameBlock &node, const string &break_label = "");
    void gen_loop(LoopBlock &node, const string &break_label);
    void gen_when(WhenBlock &node, const string &break_label = "");
    void gen_assign(AssignStmt &node);
    void gen_incr(IncrStmt &node);
    void gen_call(ProcCall &node);
    void gen_exit(ExitStmt &node);
    void gen_asm(AsmBlock &node);
    string gen_local_ref(const string &name, const string &type);
    string gen_operand(ASTNode &expr);
    string gen_lhs(ASTNode &expr);
    string gen_member(const string &object, const string &member);
};

#endif