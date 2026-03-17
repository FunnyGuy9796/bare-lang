#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "../lexer/lexer.h"

class Parser {
public:
    Parser(vector<token_t> tokens) : tokens(tokens), pos(0) {}

    unique_ptr<Program> parse();

private:
    vector<token_t> tokens;
    size_t pos;

    token_t peek();
    token_t peek_next();
    token_t consume();
    token_t expect(token_type_t type);
    bool at_end();
    bool check(token_type_t type);
    bool check_value(const string &value);

    unique_ptr<DataDecl> parse_data_decl();
    unique_ptr<SectionDecl> parse_section();
    unique_ptr<ConvDecl> parse_conv();
    unique_ptr<ProcDecl> parse_proc();
    unique_ptr<VarDecl> parse_var();
    unique_ptr<ConstDecl> parse_const();
    unique_ptr<FrameBlock> parse_frame();
    unique_ptr<LoopBlock> parse_loop();
    unique_ptr<WhenBlock> parse_when();
    unique_ptr<ASTNode> parse_statement();
    unique_ptr<ASTNode> parse_expr();
    unique_ptr<ASTNode> parse_lhs();
    unique_ptr<ASTNode> parse_asm();

    string get_type(token_type_t type);
};

#endif