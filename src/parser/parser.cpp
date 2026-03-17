#include "parser.h"

token_t Parser::peek() {
    return tokens[pos];
}

token_t Parser::peek_next() {
    if (pos + 1 < tokens.size())
        return tokens[pos + 1];
    
    return tokens[pos];
}

token_t Parser::consume() {
    return tokens[pos++];
}

token_t Parser::expect(token_type_t type) {
    if (tokens[pos].type != type)
        throw runtime_error("error on line " + to_string(tokens[pos].line) + ": expected '" + get_type(type) + "' but got '" + tokens[pos].value + "'");
    
    return tokens[pos++];
}

bool Parser::check(token_type_t type) {
    return !at_end() && tokens[pos].type == type;
}

bool Parser::check_value(const string &value) {
    return !at_end() && tokens[pos].value == value;
}

bool Parser::at_end() {
    return pos >= tokens.size();
}

unique_ptr<DataDecl> Parser::parse_data_decl() {
    expect(KEYWORD);

    auto node = make_unique<DataDecl>();

    node->name = expect(IDENT).value;

    expect(COLON);

    while (!check_value("end")) {
        auto field = make_unique<FieldDecl>();

        field->name = expect(IDENT).value;

        expect(COLON);

        field->type = expect(TYPE).value;
        node->fields.push_back(move(field));
    }

    expect(KEYWORD);

    return node;
}

unique_ptr<SectionDecl> Parser::parse_section() {
    expect(KEYWORD);

    auto node = make_unique<SectionDecl>();

    node->name = expect(SECTION_NAME).value;

    expect(COLON);
    
    while (!at_end() && !check_value("section")) {
        if (check_value("conv"))
            node->contents.push_back(parse_conv());
        else if (check_value("proc"))
            node->contents.push_back(parse_proc());
        else if (check_value("var"))
            node->contents.push_back(parse_var());
        else if (check_value("const"))
            node->contents.push_back(parse_const());
        else
            throw runtime_error("error on line " + to_string(peek().line) + ": unexpected token '" + peek().value + "' in section");
    }

    return node;
}

unique_ptr<ConvDecl> Parser::parse_conv() {
    expect(KEYWORD);

    auto node = make_unique<ConvDecl>();

    node->name = expect(IDENT).value;

    expect(COLON);
    expect(KEYWORD);
    expect(COLON);

    if (check_value("null"))
        consume();
    else {
        while (check_value("reg")) {
            expect(KEYWORD);

            node->arg_regs.push_back(expect(REG_NAME).value);
        }
    }

    expect(KEYWORD);
    expect(COLON);

    if (check_value("null"))
        consume();
    else {
        while (check_value("reg")) {
            expect(KEYWORD);

            node->ret_regs.push_back(expect(REG_NAME).value);
        }
    }

    return node;
}

unique_ptr<ProcDecl> Parser::parse_proc() {
    expect(KEYWORD);

    auto node = make_unique<ProcDecl>();

    node->name = expect(IDENT).value;

    expect(LPAREN);

    while (!check(RPAREN)) {
        auto param = make_unique<FieldDecl>();

        param->name = expect(IDENT).value;

        expect(COLON);

        param->type = expect(TYPE).value;

        node->params.push_back(move(param));
    }

    expect(RPAREN);
    expect(ARROW);
    expect(LPAREN);

    while (!check(RPAREN)) {
        auto ret = make_unique<FieldDecl>();

        ret->name = expect(IDENT).value;

        expect(COLON);

        ret->type = expect(TYPE).value;

        node->rets.push_back(move(ret));
    }

    expect(RPAREN);
    
    if (check_value("using")) {
        expect(KEYWORD);

        node->calling_conv = expect(IDENT).value;
    }

    expect(COLON);

    while (!at_end()) {
        if (check_value("ret")) {
            expect(KEYWORD);

            node->body.push_back(make_unique<RetStmt>());

            break;
        }

        node->body.push_back(parse_statement());
    }

    return node;
}

unique_ptr<VarDecl> Parser::parse_var() {
    expect(KEYWORD);

    auto node = make_unique<VarDecl>();

    node->name = expect(IDENT).value;

    expect(COLON);

    if (check(TYPE))
        node->type = expect(TYPE).value;
    else
        node->type = expect(IDENT).value;
    
    if (check(LBRACK)) {
        expect(LBRACK);

        node->is_array = true;
        node->array_size = stoi(expect(INT_LITERAL).value);

        expect(RBRACK);
    }

    return node;
}

unique_ptr<ConstDecl> Parser::parse_const() {
    expect(KEYWORD);

    auto node = make_unique<ConstDecl>();

    node->name = expect(IDENT).value;

    expect(COLON);

    node->type = expect(TYPE).value;

    expect(ASSIGN);

    node->value = parse_expr();

    return node;
}

unique_ptr<FrameBlock> Parser::parse_frame() {
    expect(KEYWORD);
    expect(COLON);

    auto node = make_unique<FrameBlock>();

    while (!check_value("end"))
        node->body.push_back(parse_statement());
    
    expect(KEYWORD);

    return node;
}

unique_ptr<LoopBlock> Parser::parse_loop() {
    expect(KEYWORD);
    expect(COLON);

    auto node = make_unique<LoopBlock>();

    while (!check_value("end"))
        node->body.push_back(parse_statement());
    
    expect(KEYWORD);

    return node;
}

unique_ptr<WhenBlock> Parser::parse_when() {
    expect(KEYWORD);

    auto node = make_unique<WhenBlock>();

    node->condition = parse_expr();

    expect(COLON);

    while (!check_value("end"))
        node->body.push_back(parse_statement());
    
    expect(KEYWORD);

    return node;
}

unique_ptr<ASTNode> Parser::parse_statement() {
    if (check_value("frame"))
        return parse_frame();
    
    if (check_value("loop"))
        return parse_loop();
    
    if (check_value("when"))
        return parse_when();
    
    if (check_value("var"))
        return parse_var();
    
    if (check_value("asm"))
        return parse_asm();
    
    if (check_value("break")) {
        expect(KEYWORD);

        return make_unique<BreakStmt>();
    }

    if (check_value("ret")) {
        expect(KEYWORD);

        return make_unique<RetStmt>();
    }

    if (check_value("exit")) {
        expect(KEYWORD);

        auto node = make_unique<ExitStmt>();

        node->code = parse_expr();

        return node;
    }

    if (check(IDENT) && peek_next().type == LPAREN) {
        auto node = make_unique<ProcCall>();

        node->name = expect(IDENT).value;

        expect(LPAREN);
        expect(RPAREN);

        return node;
    }

    if (check(IDENT) && peek_next().type == OPERATOR) {
        auto node = make_unique<IncrStmt>();

        node->name = expect(IDENT).value;
        
        string op = expect(OPERATOR).value;

        node->is_dec = (op == "--");

        return node;
    }

    if (check_value("reg")) {
        auto lhs = parse_lhs();

        expect(ASSIGN);

        auto rhs = parse_expr();
        auto node = make_unique<AssignStmt>();

        node->lhs = move(lhs);
        node->rhs = move(rhs);

        return node;
    }

    if (check(IDENT)) {
        auto lhs = parse_lhs();

        expect(ASSIGN);

        auto rhs = parse_expr();
        auto node = make_unique<AssignStmt>();

        node->lhs = move(lhs);
        node->rhs = move(rhs);

        return node;
    }

    throw runtime_error("error on line " + to_string(peek().line) + ": unexpected token '" + peek().value + "' in statement");
}

unique_ptr<ASTNode> Parser::parse_expr() {
    unique_ptr<ASTNode> left;

    if (check_value("reg")) {
        expect(KEYWORD);

        auto node = make_unique<RegOperand>();

        node->name = expect(REG_NAME).value;
        left = move(node);
    } else if (check(HEX_LITERAL)) {
        auto node = make_unique<HexLiteral>();

        node->value = stoull(consume().value, nullptr, 16);
        left = move(node);
    } else if (check(INT_LITERAL)) {
        auto node = make_unique<IntLiteral>();

        node->value = stoull(consume().value);
        left = move(node);
    } else if (check(IDENT) && peek_next().type == DOT) {
        auto node = make_unique<MemberAccess>();

        node->object = expect(IDENT).value;

        expect(DOT);

        node->member = expect(IDENT).value;
        left = move(node);
    } else if (check(IDENT) && peek_next().type == LBRACK) {
        auto node = make_unique<ArrayAccess>();

        node->name = expect(IDENT).value;

        expect(LBRACK);

        node->index = parse_expr();

        expect(RBRACK);

        left = move(node);
    } else if (check(IDENT)) {
        auto node = make_unique<Identifier>();

        node->name = consume().value;
        left = move(node);
    } else
        throw runtime_error("error on line " + to_string(peek().line) + ": unexpected token '" + peek().value + "' in expression");
    
    if (check(OPERATOR)) {
        auto node = make_unique<BinaryExpr>();

        node->op = consume().value;
        node->left = move(left);
        node->right = parse_expr();

        return node;
    }

    return left;
}

unique_ptr<ASTNode> Parser::parse_lhs() {
    if (check_value("reg")) {
        expect(KEYWORD);

        auto node = make_unique<RegOperand>();

        node->name = expect(REG_NAME).value;

        return node;
    }

    if (check(IDENT) && peek_next().type == DOT) {
        auto node = make_unique<MemberAccess>();

        node->object = expect(IDENT).value;

        expect(DOT);

        node->member = expect(IDENT).value;

        return node;
    }

    if (check(IDENT) && peek_next().type == LBRACK) {
        auto node = make_unique<ArrayAccess>();

        node->name = expect(IDENT).value;

        expect(LBRACK);

        node->index = parse_expr();

        expect(RBRACK);

        return node;
    }

    if (check(IDENT)) {
        auto node = make_unique<Identifier>();

        node->name = consume().value;

        return node;
    }

    throw runtime_error("error on line " + to_string(peek().line) + ": unexpected token '" + peek().value + "' in lhs");
}

unique_ptr<ASTNode> Parser::parse_asm() {
    expect(KEYWORD);
    expect(COLON);

    auto node = make_unique<AsmBlock>();

    while (!check_value("end")) {
        token_t tok = expect(STRING_LITERAL);

        node->lines.push_back(tok.value);
    }

    expect(KEYWORD);

    return node;
}

string Parser::get_type(token_type_t type) {
    switch (type) {
        case KEYWORD:
            return "KEYWORD";
        
        case IDENT:
            return "IDENT";
        
        case TYPE:
            return "TYPE";
        
        case ARROW:
            return "->";
        
        case COLON:
            return ":";
        
        case DOT:
            return ".";
        
        case LPAREN:
            return "(";
        
        case RPAREN:
            return ")";
        
        case LBRACK:
            return "[";
        
        case RBRACK:
            return "]";
        
        case OPERATOR:
            return "OPERATOR";
        
        case REG_NAME:
            return "REG_NAME";
        
        case ASSIGN:
            return "=";
        
        case SECTION_NAME:
            return "SECTION_NAME";
        
        case INT_LITERAL:
            return "INT_LITERAL";
        
        case HEX_LITERAL:
            return "HEX_LITERAL";
    }
}

unique_ptr<Program> Parser::parse() {
    auto program = make_unique<Program>();

    while (!at_end()) {
        if (check_value("data"))
            program->data_decls.push_back(parse_data_decl());
        else if (check_value("section"))
            program->sections.push_back(parse_section());
        else
            throw runtime_error("error on line " + to_string(peek().line) + ": unexpected token '" + peek().value + "' at top level");
    }

    return program;
}