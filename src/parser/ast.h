#ifndef AST_H
#define AST_H

#include <string>
#include <vector>
#include <memory>

using namespace std;

struct ASTNode {
    virtual ~ASTNode() = default;
};

struct FieldDecl : ASTNode {
    string name;
    string type;
};

struct DataDecl : ASTNode {
    string name;
    vector<unique_ptr<FieldDecl>> fields;
};

struct ConvDecl : ASTNode {
    string name;
    vector<string> arg_regs;
    vector<string> ret_regs;
};

struct ProcDecl : ASTNode {
    string name;
    vector<unique_ptr<FieldDecl>> params;
    vector<unique_ptr<FieldDecl>> rets;
    string calling_conv;
    vector<unique_ptr<ASTNode>> body;
};

struct VarDecl : ASTNode {
    string name;
    string type;
    bool is_array = false;
    int array_size = 0;
    unique_ptr<ASTNode> initializer;
};

struct ConstDecl : ASTNode {
    string name;
    string type;
    unique_ptr<ASTNode> value;
};

struct SectionDecl : ASTNode {
    string name;
    vector<unique_ptr<ASTNode>> contents;
};

struct FrameBlock : ASTNode {
    vector<unique_ptr<ASTNode>> body;
};

struct LoopBlock : ASTNode {
    vector<unique_ptr<ASTNode>> body;
};

struct WhenBlock : ASTNode {
    unique_ptr<ASTNode> condition;
    vector<unique_ptr<ASTNode>> body;
};

struct RetStmt : ASTNode {};

struct BreakStmt : ASTNode {};

struct AssignStmt : ASTNode {
    unique_ptr<ASTNode> lhs;
    unique_ptr<ASTNode> rhs;
};

struct IncrStmt : ASTNode {
    string name;
    bool is_dec = false;
};

struct ProcCall : ASTNode {
    string name;
};

struct BinaryExpr : ASTNode {
    string op;
    unique_ptr<ASTNode> left;
    unique_ptr<ASTNode> right;
};

struct RegOperand : ASTNode {
    string name;
};

struct MemberAccess : ASTNode {
    string object;
    string member;
};

struct ArrayAccess : ASTNode {
    string name;
    unique_ptr<ASTNode> index;
};

struct Identifier : ASTNode {
    string name;
};

struct IntLiteral : ASTNode {
    uint64_t value;
};

struct HexLiteral : ASTNode {
    uint64_t value;
};

struct AsmBlock : ASTNode {
    vector<string> lines;
};

struct ExitStmt : ASTNode {
    unique_ptr<ASTNode> code;
};

struct Program : ASTNode {
    vector<unique_ptr<DataDecl>> data_decls;
    vector<unique_ptr<SectionDecl>> sections;
};

#endif