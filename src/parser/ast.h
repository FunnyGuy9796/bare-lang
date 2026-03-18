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

struct ProcDecl : ASTNode {
    string name;
    vector<unique_ptr<FieldDecl>> params;
    vector<unique_ptr<FieldDecl>> rets;
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
    string str_value;
    bool is_string = false;
};

struct SectionDecl : ASTNode {
    string name;
    string attributes;
    vector<unique_ptr<ASTNode>> contents;
};

struct MemRef : ASTNode {
    string type;
    string segment;
    unique_ptr<ASTNode> address;
    unique_ptr<ASTNode> offset;
};

struct AddrOf : ASTNode {
    string name;
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

struct UnaryExpr : ASTNode {
    string op;
    unique_ptr<ASTNode> operand;
};

struct RegOperand : ASTNode {
    string name;
};

struct SegOperand : ASTNode {
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

struct SyscallStmt : ASTNode {};

struct DerefStmt : ASTNode {
    string type;
    unique_ptr<ASTNode> ptr;
    unique_ptr<ASTNode> offset;
};

struct InStmt : ASTNode {
    unique_ptr<ASTNode> port;
    unique_ptr<ASTNode> dest;
};

struct OutStmt : ASTNode {
    unique_ptr<ASTNode> port;
    unique_ptr<ASTNode> value;
};

struct CliStmt : ASTNode {};

struct StiStmt : ASTNode {};

struct HltStmt : ASTNode {};

struct BitsStmt : ASTNode {
    int width;
};

struct RawData : ASTNode {
    string directive;
    vector<unique_ptr<ASTNode>> values;
};

struct FillStmt : ASTNode {
    unique_ptr<ASTNode> target;
    unique_ptr<ASTNode> value;
};

struct StringLiteral : ASTNode {
    string value;
};

struct OrgStmt : ASTNode {
    uint32_t address;
};

struct Program : ASTNode {
    vector<unique_ptr<DataDecl>> data_decls;
    vector<unique_ptr<SectionDecl>> sections;
};

#endif