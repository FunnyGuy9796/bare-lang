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

    if (check(IDENT) && peek().value == "follows") {
        consume();
        expect(LPAREN);

        string follows_name = consume().value;

        expect(RPAREN);

        node->attributes = "follows=" + follows_name;
    }

    size_t start = node->attributes.find_first_not_of(" \t\n\r");
    size_t end = node->attributes.find_last_not_of(" \t\n\r");
    
    if (start != string::npos)
        node->attributes = node->attributes.substr(start, end - start + 1);
    else
        node->attributes = "";

    expect(COLON);
    
    while (!at_end() && !check_value("section")) {
        if (check_value("proc"))
            node->contents.push_back(parse_proc());
        else if (check_value("var"))
            node->contents.push_back(parse_var());
        else if (check_value("const"))
            node->contents.push_back(parse_const());
        else if (check_value("bits")) {
            expect(KEYWORD);

            auto bits = make_unique<BitsStmt>();

            bits->width = stoi(expect(INT_LITERAL).value);
            node->contents.push_back(move(bits));
        } else if (check_value("org")) {
            expect(KEYWORD);

            auto org = make_unique<OrgStmt>();

            if (check(HEX_LITERAL))
                org->address = stoull(consume().value, nullptr, 16);
            else
                org->address = stoull(expect(INT_LITERAL).value);

            node->contents.push_back(move(org));
        } else if (check_value("db") || check_value("dw") || check_value("dd") || check_value("dq")) {
            string dir = consume().value;
            auto raw = make_unique<RawData>();

            raw->directive = dir;
            raw->values.push_back(parse_expr());

            while (check(COMMA)) {
                expect(COMMA);

                raw->values.push_back(parse_expr());
            }

            node->contents.push_back(move(raw));
        } else if (check_value("asm"))
            node->contents.push_back(parse_asm());
        else if (check_value("fill")) {
            expect(KEYWORD);

            auto fill = make_unique<FillStmt>();

            fill->target = parse_expr();

            expect(COMMA);

            fill->value = parse_expr();
            node->contents.push_back(move(fill));
        } else
            throw runtime_error("error on line " + to_string(peek().line) + ": unexpected token '" + peek().value + "' in section");
    }

    return node;
}

unique_ptr<ProcDecl> Parser::parse_proc() {
    expect(KEYWORD);

    auto node = make_unique<ProcDecl>();

    node->name = expect(IDENT).value;

    expect(LPAREN);
    expect(RPAREN);
    expect(COLON);

    while (!at_end()) {
        if (check_value("ret")) {
            expect(KEYWORD);

            node->body.push_back(make_unique<RetStmt>());

            break;
        }

        if (check_value("section") || check_value("proc") || check_value("var") || check_value("const") || check_value("bits") || check_value("db")
            || check_value("dw") || check_value("dd") || check_value("dq") || check_value("fill") || check_value("org"))
            break;

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

    if (check(STRING_LITERAL)) {
        node->is_string = true;
        node->str_value = consume().value;
    } else
        node->value = parse_expr();

    return node;
}

unique_ptr<MemRef> Parser::parse_memref() {
    auto node = make_unique<MemRef>();

    node->type = "u32";
    node->segment = "";

    if (check(TYPE)) {
        node->type = expect(TYPE).value;

        expect(COLON);
    }

    if (check(SEG_NAME)) {
        node->segment = consume().value;
        
        expect(COLON);
    }

    if (check_value("reg")) {
        expect(KEYWORD);

        auto reg = make_unique<RegOperand>();

        reg->name = expect(REG_NAME).value;
        node->address = move(reg);
    } else if (check(HEX_LITERAL)) {
        auto lit = make_unique<HexLiteral>();

        lit->value = stoull(consume().value, nullptr, 16);
        node->address = move(lit);
    } else if (check(INT_LITERAL)) {
        auto lit = make_unique<IntLiteral>();

        lit->value = stoull(consume().value);
        node->address = move(lit);
    } else if (check(IDENT)) {
        auto ident = make_unique<Identifier>();

        ident->name = consume().value;
        node->address = move(ident);
    } else
        throw runtime_error("error on line " + to_string(peek().line) + ": expected address after '&'");

    if (check(OPERATOR) && (peek().value == "+" || peek().value == "-")) {
        string op = consume().value;
        auto offset_expr = parse_expr();

        if (op == "-") {
            auto zero = make_unique<IntLiteral>();

            zero->value = 0;

            auto neg = make_unique<BinaryExpr>();

            neg->op = "-";
            neg->left = move(zero);
            neg->right = move(offset_expr);
            node->offset = move(neg);
        } else
            node->offset = move(offset_expr);
    }

    return node;
}

unique_ptr<DerefStmt> Parser::parse_deref() {
    auto node = make_unique<DerefStmt>();

    node->type = "u32";

    if (check(TYPE)) {
        node->type = expect(TYPE).value;

        expect(COLON);
    }

    if (check_value("reg")) {
        expect(KEYWORD);

        auto reg = make_unique<RegOperand>();

        reg->name = expect(REG_NAME).value;
        node->ptr = move(reg);
    } else if (check(IDENT)) {
        auto ident = make_unique<Identifier>();

        ident->name = consume().value;
        node->ptr = move(ident);
    } else
        throw runtime_error("error on line " + to_string(peek().line) + ": expected pointer after '*'");
    
    if (check(OPERATOR) && (peek().value == "+" || peek().value == "-")) {
        string op = consume().value;

        node->offset = parse_expr();

        if (op == "-") {
            auto zero = make_unique<IntLiteral>();

            zero->value = 0;

            auto neg = make_unique<BinaryExpr>();

            neg->op = "-";
            neg->left = move(zero);
            neg->right = move(node->offset);
            node->offset = move(neg);
        }
    }

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

    if (check_value("syscall")) {
        expect(KEYWORD);

        return make_unique<SyscallStmt>();
    }

    if (check_value("out")) {
        expect(KEYWORD);

        auto node = make_unique<OutStmt>();

        node->port = parse_expr();

        expect(COMMA);

        node->value = parse_expr();

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

    if (check_value("seg")) {
        auto lhs = parse_lhs();

        expect(ASSIGN);

        auto rhs = parse_expr();
        auto node = make_unique<AssignStmt>();

        node->lhs = move(lhs);
        node->rhs = move(rhs);

        return node;
    }

    if (check_value("cli")) {
        expect(KEYWORD);

        return make_unique<CliStmt>();
    }

    if (check_value("sti")) {
        expect(KEYWORD);

        return make_unique<StiStmt>();
    }

    if (check_value("hlt")) {
        expect(KEYWORD);

        return make_unique<HltStmt>();
    }

    if (check(AMPERSAND)) {
        auto lhs = parse_lhs();

        expect(ASSIGN);

        auto rhs = parse_expr();
        auto node = make_unique<AssignStmt>();

        node->lhs = move(lhs);
        node->rhs = move(rhs);

        return node;
    }

    if (check(STAR)) {
        expect(STAR);

        auto lhs = parse_deref();

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
    } else if (check_value("seg")) {
        expect(KEYWORD);

        auto node = make_unique<SegOperand>();

        node->name = expect(SEG_NAME).value;
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
    } else if (check(AMPERSAND)) {
        expect(AMPERSAND);

        left = parse_memref();
    } else if (check(STAR)) {
        expect(STAR);

        left = parse_deref();
    } else if (check(OPERATOR) && peek().value == "~") {
        consume();

        auto node = make_unique<UnaryExpr>();

        node->op = "~";
        node->operand = parse_expr();
        left = move(node);
    } else if (check(STRING_LITERAL)) {
        auto node = make_unique<StringLiteral>();

        node->value = consume().value;
        left = move(node);
    } else if (check_value("addr")) {
        expect(KEYWORD);

        auto node = make_unique<AddrOf>();

        node->name = expect(IDENT).value;
        left = move(node);
    } else if (check_value("in")) {
        expect(KEYWORD);

        auto node = make_unique<InStmt>();

        node->port = parse_expr();
        left = move(node);
    } else
        throw runtime_error("error on line " + to_string(peek().line) + ": unexpected token '" + peek().value + "' in expression");
    
    uint32_t left_line = tokens[pos > 0 ? pos - 1 : 0].line;
    
    if (check(AMPERSAND) && peek().line == left_line) {
        auto node = make_unique<BinaryExpr>();

        node->op = "&";

        consume();

        node->left = move(left);
        node->right = parse_expr();

        return node;
    }
    
    if (check(OPERATOR) && peek().line == left_line) {
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

    if (check_value("seg")) {
        expect(KEYWORD);

        auto node = make_unique<SegOperand>();

        node->name = expect(SEG_NAME).value;

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

    if (check(AMPERSAND)) {
        expect(AMPERSAND);

        return parse_memref();
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
        
        case COLON:
            return ":";
        
        case DOT:
            return ".";
        
        case COMMA:
            return ",";
        
        case LPAREN:
            return "(";
        
        case RPAREN:
            return ")";
        
        case LBRACK:
            return "[";
        
        case RBRACK:
            return "]";
        
        case ASSIGN:
            return "=";

        case AMPERSAND:
            return "&";
        
        case OPERATOR:
            return "OPERATOR";
        
        case REG_NAME:
            return "REG_NAME";
        
        case SEG_NAME:
            return "SEG_NAME";
        
        case SECTION_NAME:
            return "SECTION_NAME";
        
        case INT_LITERAL:
            return "INT_LITERAL";
        
        case HEX_LITERAL:
            return "HEX_LITERAL";

        case STRING_LITERAL:
            return "STRING_LITERAL";
        
        default:
            return "UNKOWN";
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