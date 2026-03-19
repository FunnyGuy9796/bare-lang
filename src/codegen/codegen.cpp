#include "codegen.h"

string CodeGen::make_label(const string &prefix) {
    return prefix + "_" + to_string(label_count++);
}

int CodeGen::type_size(const string &type) {
    if (type == "u1" || type == "u8" || type == "i8")
        return 1;
    
    if (type == "u16" || type == "i16")
        return 2;
    
    if (type == "u32" || type == "i32")
        return 4;
    
    if (type == "u64" || type == "i64")
        return 8;
    
    if (data_type_sizes.count(type))
        return data_type_sizes[type];
    
    return 4;
}

string CodeGen::size_word(const string &type) {
    int size = type_size(type);

    if (size == 1)
        return "byte";
    
    if (size == 2)
        return "word";
    
    if (size == 4)
        return "dword";
    
    if (size == 8)
        return "qword";
    
    return "dword";
}

string CodeGen::generate(Program &program) {
    gen_program(program);

    return out.str();
}

void CodeGen::gen_program(Program &program) {
    for (const auto &data : program.data_decls) {
        int offset = 0;

        for (const auto &field : data->fields) {
            field_offsets[data->name][field->name] = offset;
            field_types[data->name + "." + field->name] = field->type;
            offset += type_size(field->type);
        }
        
        data_type_sizes[data->name] = offset;
    }

    for (const auto &section : program.sections)
        gen_section(*section);
}

void CodeGen::gen_section(SectionDecl &section) {
    for (const auto &node : section.contents) {
        if (auto *org = dynamic_cast<OrgStmt *>(node.get())) {
            stringstream ss;

            ss << "0x" << hex << org->address;
            out << "org " << ss.str() << "\n";
        }
    }

    out << "section " << section.name;

    if (!section.attributes.empty())
        out << " " << section.attributes;

    out << "\n";

    for (const auto &node : section.contents) {
        if (dynamic_cast<OrgStmt *>(node.get()))
            continue;
        else if (auto *bits = dynamic_cast<BitsStmt *>(node.get()))
            out << "bits " << bits->width << "\n";
        else if (auto *var = dynamic_cast<VarDecl *>(node.get())) {
            if (var->initializer)
                gen_var_data(*var);
            else
                gen_var_bss(*var);
        } else if (auto *cn = dynamic_cast<ConstDecl *>(node.get()))
            gen_const(*cn);
        else if (auto *proc = dynamic_cast<ProcDecl *>(node.get()))
            gen_proc(*proc);
        else if (auto *raw = dynamic_cast<RawData *>(node.get()))
            gen_raw_data(*raw);
        else if (auto *a = dynamic_cast<AsmBlock *>(node.get()))
            gen_asm(*a);
        else if (auto *fill = dynamic_cast<FillStmt *>(node.get()))
            gen_fill(*fill);
    }
}

void CodeGen::gen_var_data(VarDecl &decl) {
    string dbX;
    int size = type_size(decl.type);

    if (size == 1)
        dbX = "db";
    else if (size == 2)
        dbX = "dw";
    else if (size == 4)
        dbX = "dd";
    else if (size == 8)
        dbX = "dq";
    else
        dbX = "dd";

    string val = gen_operand(*decl.initializer);

    if (decl.is_array)
        out << "    " << decl.name << ": times " << decl.array_size << " " << dbX << " " << val << "\n";
    else
        out << "    " << decl.name << ": " << dbX << " " << val << "\n";
}

void CodeGen::gen_var_bss(VarDecl &decl) {
    int total_bytes;

    if (decl.is_array)
        total_bytes = decl.array_size * type_size(decl.type);
    else
        total_bytes = type_size(decl.type);

    out << "    " << decl.name << ": resb " << total_bytes << "\n";
}

void CodeGen::gen_const(ConstDecl &decl) {
    if (decl.is_string) {
        out << decl.name << ": db \"" << decl.str_value << "\", 0\n";

        return;
    }

    string dbX;
    int size = type_size(decl.type);

    if (size == 1)
        dbX = "db";
    else if (size == 2)
        dbX = "dw";
    else if (size == 4)
        dbX = "dd";
    else if (size == 8)
        dbX = "dq";
    else
        dbX = "dd";

    string val = gen_operand(*decl.value);

    out << "    " << decl.name << ": " << dbX << " " << val << "\n";
}

void CodeGen::gen_proc(ProcDecl &decl) {
    if (decl.name == "_start")
        out << "global _start\n";
    
    out << decl.name << ":\n";

    for (const auto &stmt : decl.body)
        gen_statement(*stmt, "");
}

void CodeGen::gen_statement(ASTNode &node, const string &break_label) {
    if (auto *frame = dynamic_cast<FrameBlock *>(&node))
        gen_frame(*frame, break_label);
    else if (auto *loop = dynamic_cast<LoopBlock *>(&node)) {
        string new_break = make_label("break");

        gen_loop(*loop, new_break);
    } else if (auto *when = dynamic_cast<WhenBlock *>(&node))
        gen_when(*when, break_label);
    else if (auto *assign = dynamic_cast<AssignStmt *>(&node))
        gen_assign(*assign);
    else if (auto *incr = dynamic_cast<IncrStmt *>(&node))
        gen_incr(*incr);
    else if (auto *call = dynamic_cast<ProcCall *>(&node))
        gen_call(*call);
    else if (auto *a = dynamic_cast<AsmBlock *>(&node))
        gen_asm(*a);
    else if (auto *o = dynamic_cast<OutStmt *>(&node))
        gen_out(*o);
    else if (dynamic_cast<SyscallStmt *>(&node))
        out << "    int 0x80\n";
    else if (dynamic_cast<RetStmt *>(&node))
        out << "    ret\n";
    else if (dynamic_cast<BreakStmt *>(&node)) {
        if (!break_label.empty())
            out << "    jmp " << break_label << "\n";
    } else if (dynamic_cast<CliStmt *>(&node))
        out << "    cli\n";
    else if (dynamic_cast<StiStmt *>(&node))
        out << "    sti\n";
    else if (dynamic_cast<HltStmt *>(&node))
        out << "    hlt\n";
}

void CodeGen::gen_frame(FrameBlock &node, const string &break_label) {
    locals = SymbolTable();
    locals.push_scope();
    local_offsets.clear();
    curr_frame_offset = 0;

    int frame_size = 0;

    for (const auto &stmt : node.body) {
        if (auto *var = dynamic_cast<VarDecl *>(stmt.get())) {
            int size = type_size(var->type);

            if (var->is_array)
                size *= var->array_size;
            
            frame_size += size;
            curr_frame_offset -= size;
            local_offsets[var->name] = curr_frame_offset;

            symbol_t sym;

            sym.kind = symbol_t::Kind::VAR;
            sym.name = var->name;
            sym.type = var->type;
            sym.is_array = var->is_array;
            sym.array_size = var->array_size;

            locals.declare(var->name, sym);
        }
    }

    out << "    push ebp\n";
    out << "    mov ebp, esp\n";
    out << "    sub esp, " << frame_size << "\n";

    for (const auto &stmt : node.body)
        gen_statement(*stmt, break_label);
    
    out << "    mov esp, ebp\n";
    out << "    pop ebp\n";

    locals.pop_scope();
}

void CodeGen::gen_loop(LoopBlock &node, const string &break_label) {
    string loop_label = make_label("loop");

    out << loop_label << ":\n";

    for (const auto &stmt : node.body)
        gen_statement(*stmt, break_label);

    out << "    jmp " << loop_label << "\n";
    out << break_label << ":\n";
}

void CodeGen::gen_when(WhenBlock &node, const string &break_label) {
    string end_label = make_label("when_end");
    string else_label = make_label("when_else");

    if (auto *bin = dynamic_cast<BinaryExpr *>(node.condition.get())) {
        string left  = gen_operand(*bin->left);
        string right = gen_operand(*bin->right);

        out << "    cmp " << left << ", " << right << "\n";

        if (!node.else_body.empty()) {
            if (bin->op == "==")
                out << "    jne " << else_label << "\n";
            else if (bin->op == "!=")
                out << "    je "  << else_label << "\n";
            else if (bin->op == ">=")
                out << "    jl "  << else_label << "\n";
            else if (bin->op == "<=")
                out << "    jg "  << else_label << "\n";
            else if (bin->op == ">")
                out << "    jle " << else_label << "\n";
            else if (bin->op == "<")
                out << "    jge " << else_label << "\n";
        } else {
            if (bin->op == "==")
                out << "    jne " << end_label << "\n";
            else if (bin->op == "!=")
                out << "    je "  << end_label << "\n";
            else if (bin->op == ">=")
                out << "    jl "  << end_label << "\n";
            else if (bin->op == "<=")
                out << "    jg "  << end_label << "\n";
            else if (bin->op == ">")
                out << "    jle " << end_label << "\n";
            else if (bin->op == "<")
                out << "    jge " << end_label << "\n";
        }
    }

    for (const auto &stmt : node.body)
        gen_statement(*stmt, break_label);

    if (!node.else_body.empty()) {
        out << "    jmp " << end_label << "\n";
        out << else_label << ":\n";

        for (const auto &stmt : node.else_body)
            gen_statement(*stmt, break_label);
    }

    out << end_label << ":\n";
}

void CodeGen::gen_assign(AssignStmt &node) {
    string rhs_type = "";

    if (auto *ident = dynamic_cast<Identifier *>(node.rhs.get())) {
        auto *sym = locals.lookup(ident->name);

        if (!sym)
            sym = globals.lookup(ident->name);

        if (sym && data_type_sizes.count(sym->type))
            rhs_type = sym->type;
    }

    if (!rhs_type.empty() && field_offsets.count(rhs_type)) {
        string lhs_base = gen_lhs(*node.lhs);
        string lhs_addr = lhs_base;

        for (const string &sw : {"byte ", "word ", "dword ", "qword "}) {
            if (lhs_addr.rfind(sw, 0) == 0) {
                lhs_addr = lhs_addr.substr(sw.size());

                break;
            }
        }

        out << "    lea edi, " << lhs_addr << "\n";

        auto *rhs_ident = dynamic_cast<Identifier *>(node.rhs.get());

        for (const auto &[fname, foffset] : field_offsets[rhs_type]) {
            string ftype = field_types[rhs_type + "." + fname];
            string fsw = size_word(ftype);
            string src = gen_member(rhs_ident->name, fname);

            out << "    mov eax, " << src << "\n";
            out << "    mov " << fsw << " [edi + " << foffset << "], eax\n";
        }

        return;
    }

    string lhs = gen_lhs(*node.lhs);
    string lhs_type = "u32";

    if (auto *memref = dynamic_cast<MemRef *>(node.lhs.get()))
        lhs_type = memref->type;
    else if (auto *reg = dynamic_cast<RegOperand *>(node.lhs.get())) {
        if (reg->name.back() == 'l' || reg->name.back() == 'h')
            lhs_type = "u8";
        else if (reg->name.size() == 2 && reg->name[0] != 'e')
            lhs_type = "u16";
    }

    string rhs = gen_operand(*node.rhs, lhs_type);

    if (lhs == rhs)
        return;

    bool src_is_byte = rhs.size() >= 4 && rhs.substr(0, 4) == "byte";
    bool src_is_word = rhs.size() >= 4 && rhs.substr(0, 4) == "word";
    bool dest_is_32bit_reg = (lhs == "eax" || lhs == "ebx" || lhs == "ecx" ||
                            lhs == "edx" || lhs == "esi" || lhs == "edi" ||
                            lhs == "esp" || lhs == "ebp");

    if (dest_is_32bit_reg && (src_is_byte || src_is_word))
        out << "    movzx " << lhs << ", " << rhs << "\n";
    else
        out << "    mov " << lhs << ", " << rhs << "\n";
}

void CodeGen::gen_incr(IncrStmt &node) {
    string op = node.is_dec ? "dec" : "inc";

    if (node.is_reg) {
        out << "    " << op << " " << node.reg_name << "\n";

        return;
    }

    auto *sym = locals.lookup(node.name);

    if (!sym)
        sym = globals.lookup(node.name);

    string sw = sym ? size_word(sym->type) : "dword";

    out << "    " << op << " " << sw << " [" << node.name << "]\n";
}

void CodeGen::gen_call(ProcCall &node) {
    out << "    call " << node.name << "\n";
}

void CodeGen::gen_asm(AsmBlock &node) {
    for (const auto &line : node.lines)
        out << "    " << line << "\n";
}

void CodeGen::gen_out(OutStmt &node) {
    string port = gen_operand(*node.port);
    string value = gen_operand(*node.value);

    if (dynamic_cast<IntLiteral *>(node.port.get()) || dynamic_cast<HexLiteral *>(node.port.get()))
        out << "    out " << port << ", " << value << "\n";
    else {
        out << "    mov dx, " << port << "\n";
        out << "    out dx, " << value << "\n";
    }
}

string CodeGen::gen_operand(ASTNode &expr, const string &size_hint) {
    if (auto *i = dynamic_cast<IntLiteral *>(&expr))
        return to_string(i->value);
    
    if (auto *h = dynamic_cast<HexLiteral *>(&expr)) {
        stringstream ss;

        ss << "0x" << hex << h->value;

        return ss.str();
    }

    if (dynamic_cast<NullLiteral *>(&expr))
        return "0";

    if (auto *reg = dynamic_cast<RegOperand *>(&expr))
        return reg->name;
    
    if (auto *seg = dynamic_cast<SegOperand *>(&expr))
        return seg->name;
    
    if (auto *memref = dynamic_cast<MemRef *>(&expr)) {
        string sw = size_word(memref->type);
        string addr = gen_memref_addr(*memref);

        return sw + " [" + addr + "]";
    }

    if (auto *deref = dynamic_cast<DerefStmt *>(&expr)) {
        string sw = size_word(deref->type);
        string ptr_val = gen_operand(*deref->ptr);

        out << "    mov edx, " << ptr_val << "\n";

        if (deref->offset) {
            string off = gen_operand(*deref->offset);

            return sw + " [edx + " + off + "]";
        }

        return sw + " [edx]";
    }

    if (auto *addr = dynamic_cast<AddrOf *>(&expr)) {
        if (local_offsets.count(addr->name)) {
            int offset = local_offsets[addr->name];
            
            out << "    lea eax, [ebp" << (offset < 0 ? " - " : " + ") << to_string(abs(offset)) << "]\n";

            return "eax";
        }
        
        return addr->name;
    }
    
    if (auto *ident = dynamic_cast<Identifier *>(&expr)) {
        if (local_offsets.count(ident->name)) {
            auto *sym = locals.lookup(ident->name);

            return gen_local_ref(ident->name, sym->type);
        }

        auto *sym = globals.lookup(ident->name);
        string sw = sym ? size_word(sym->type) : "dword";

        return sw + " [" + ident->name + "]";
    }

    if (auto *mem = dynamic_cast<MemberAccess *>(&expr))
        return gen_member(mem->object, mem->member);

    if (auto *arr = dynamic_cast<ArrayAccess *>(&expr)) {
        auto *sym = locals.lookup(arr->name);

        if (!sym)
            sym = globals.lookup(arr->name);
        
        string sw = sym ? size_word(sym->type) : "dword";
        int elem_size = sym ? type_size(sym->type) : 4;
        string idx = gen_operand(*arr->index);
        bool idx_is_reg = (dynamic_cast<RegOperand *>(arr->index.get()) != nullptr);
        string idx_reg;

        if (idx_is_reg)
            idx_reg = idx;
        else {
            idx_reg = "ecx";
            out << "    mov ecx, " << idx << "\n";
        }

        return sw + " [" + arr->name + " + " + idx_reg + " * " + to_string(elem_size) + "]";
    }

    if (auto *unary = dynamic_cast<UnaryExpr *>(&expr)) {
        string operand = gen_operand(*unary->operand);
        string acc = size_reg("eax", size_hint);

        if (unary->op == "~") {
            out << "    mov " << acc << ", " << operand << "\n";
            out << "    not " << acc << "\n";

            return acc;
        }

        return operand;
    }

    if (auto *bin = dynamic_cast<BinaryExpr *>(&expr)) {
        string acc = size_reg("eax", size_hint);
        bool right_is_complex = !dynamic_cast<IntLiteral *>(bin->right.get()) &&
                                !dynamic_cast<HexLiteral *>(bin->right.get()) &&
                                !dynamic_cast<RegOperand *>(bin->right.get());
        string right;

        if (right_is_complex) {
            right = gen_operand(*bin->right, size_hint);

            bool right_is_byte = right.size() >= 4 && right.substr(0, 4) == "byte";
            bool right_is_word = right.size() >= 4 && right.substr(0, 4) == "word";

            if (right_is_byte || right_is_word)
                out << "    movzx ecx, " << right << "\n";
            else
                out << "    mov ecx, " << right << "\n";

            right = size_reg("ecx", size_hint);
        }

        string left = gen_operand(*bin->left, size_hint);

        if (!right_is_complex)
            right = gen_operand(*bin->right, size_hint);

        bool left_is_pure_reg = dynamic_cast<RegOperand *>(bin->left.get()) != nullptr;
        bool left_is_eax_family = (left == "eax" || left == "ax" || left == "al" || left == "ah");

        if (left_is_pure_reg && !left_is_eax_family) {
            if (bin->op == "+") {
                out << "    add " << left << ", " << right << "\n";

                return left;
            } else if (bin->op == "-") {
                out << "    sub " << left << ", " << right << "\n";

                return left;
            } else if (bin->op == "&") {
                out << "    and " << left << ", " << right << "\n";

                return left;
            } else if (bin->op == "|") {
                out << "    or "  << left << ", " << right << "\n";

                return left;
            } else if (bin->op == "^") {
                out << "    xor " << left << ", " << right << "\n";

                return left;
            }
        }

        bool needs_movzx = (left.find("byte") != string::npos ||
                            (left.find("word") != string::npos && left.find("dword") == string::npos) ||
                            left == "si" || left == "di" ||
                            left == "ax" || left == "bx" ||
                            left == "cx" || left == "dx" ||
                            left == "al" || left == "ah" ||
                            left == "bl" || left == "bh" ||
                            left == "cl" || left == "ch" ||
                            left == "dl" || left == "dh") &&
                            acc == "eax";

        if (needs_movzx)
            out << "    movzx " << acc << ", " << left << "\n";
        else
            out << "    mov " << acc << ", " << left << "\n";

        if (bin->op == "&")
            out << "    and " << acc << ", " << right << "\n";
        else if (bin->op == "|")
            out << "    or "  << acc << ", " << right << "\n";
        else if (bin->op == "^")
            out << "    xor " << acc << ", " << right << "\n";
        else if (bin->op == "+")
            out << "    add " << acc << ", " << right << "\n";
        else if (bin->op == "-")
            out << "    sub " << acc << ", " << right << "\n";
        else if (bin->op == "*")
            out << "    imul " << acc << ", " << right << "\n";
        else if (bin->op == "<<" || bin->op == ">>") {
            string instr = (bin->op == "<<") ? "shl" : "shr";

            if (dynamic_cast<IntLiteral *>(bin->right.get()))
                out << "    " << instr << " " << acc << ", " << right << "\n";
            else {
                out << "    mov cl, " << right << "\n";
                out << "    " << instr << " " << acc << ", cl\n";
            }
        }

        return acc;
    }

    if (auto *in = dynamic_cast<InStmt *>(&expr)) {
        string port = gen_operand(*in->port);
        string acc  = size_reg("eax", size_hint);

        if (dynamic_cast<IntLiteral *>(in->port.get()) || dynamic_cast<HexLiteral *>(in->port.get()))
            out << "    in " << acc << ", " << port << "\n";
        else {
            out << "    mov dx, " << port << "\n";
            out << "    in " << acc << ", dx\n";
        }

        return acc;
    }

    if (auto *cast = dynamic_cast<CastExpr *>(&expr)) {
        int dst_size = type_size(cast->type);
        string acc = size_reg("eax", cast->type);
        int src_size = 4;

        if (auto *ident = dynamic_cast<Identifier *>(cast->expr.get())) {
            auto *sym = locals.lookup(ident->name);

            if (!sym)
                sym = globals.lookup(ident->name);

            if (sym)
                src_size = type_size(sym->type);
        } else if (auto *reg = dynamic_cast<RegOperand *>(cast->expr.get())) {
            if (reg->name.back() == 'l' || reg->name.back() == 'h')
                src_size = 1;
            else if (reg->name.size() == 2 && reg->name[0] != 'e')
                src_size = 2;
        } else if (auto *mem = dynamic_cast<MemRef *>(cast->expr.get()))
            src_size = type_size(mem->type);

        string src_type = (src_size == 1) ? "u8" : (src_size == 2) ? "u16" : (src_size == 4) ? "u32" : "u64";
        string inner = gen_operand(*cast->expr, src_type);

        if (dst_size > src_size)
            out << "    movzx " << acc << ", " << inner << "\n";
        else if (dst_size < src_size) {
            string full_acc = size_reg("eax", src_type);

            out << "    mov " << full_acc << ", " << inner << "\n";
        } else {
            if (inner != acc)
                out << "    mov " << acc << ", " << inner << "\n";
        }

        return acc;
    }

    if (auto *sz = dynamic_cast<SizeofExpr *>(&expr))
        return to_string(type_size(sz->type));

    return "0";
}

string CodeGen::gen_lhs(ASTNode &expr) {
    if (auto *reg = dynamic_cast<RegOperand *>(&expr))
        return reg->name;
    
    if (auto *seg = dynamic_cast<SegOperand *>(&expr))
        return seg->name;
    
    if (auto *memref = dynamic_cast<MemRef *>(&expr)) {
        string sw = size_word(memref->type);
        string addr = gen_memref_addr(*memref);

        return sw + " [" + addr + "]";
    }

    if (auto *deref = dynamic_cast<DerefStmt *>(&expr)) {
        string sw = size_word(deref->type);
        string ptr_val = gen_operand(*deref->ptr);

        out << "    mov edx, " + ptr_val << "\n";

        if (deref->offset) {
            string off = gen_operand(*deref->offset);

            return sw + " [edx + " + off + "]";
        }

        return sw + "[edx]";
    }
    
    if (auto *ident = dynamic_cast<Identifier *>(&expr)) {
        if (local_offsets.count(ident->name)) {
            auto *sym = locals.lookup(ident->name);

            return gen_local_ref(ident->name, sym->type);
        }

        auto *sym = globals.lookup(ident->name);
        string sw = sym ? size_word(sym->type) : "dword";

        return sw + " [" + ident->name + "]";
    }

    if (auto *mem = dynamic_cast<MemberAccess *>(&expr))
        return gen_member(mem->object, mem->member);
    
    if (auto *arr = dynamic_cast<ArrayAccess *>(&expr)) {
        auto *sym = locals.lookup(arr->name);

        if (!sym)
            sym = globals.lookup(arr->name);

        string sw = sym ? size_word(sym->type) : "dword";
        int elem_size = sym ? type_size(sym->type) : 4;
        string idx = gen_operand(*arr->index);
        bool idx_is_reg = (dynamic_cast<RegOperand *>(arr->index.get()) != nullptr);
        string idx_reg;

        if (idx_is_reg)
            idx_reg = idx;
        else {
            idx_reg = "ecx";
            out << "    mov ecx, " << idx << "\n";
        }

        return sw + " [" + arr->name + " + " + idx_reg + " * " + to_string(elem_size) + "]";
    }

    return "";
}

string CodeGen::gen_local_ref(const string &name, const string &type) {
    string sw = size_word(type);
    int offset = local_offsets[name];

    if (offset < 0)
        return sw + " [ebp - " + to_string(-offset) + "]";
    
    return sw + " [ebp + " + to_string(offset) + "]";
}

string CodeGen::gen_member(const string &object, const string &member) {
    auto *sym = locals.lookup(object);
    bool is_local = sym != nullptr;

    if (!sym)
        sym = globals.lookup(object);

    if (!sym)
        return "[" + object + "]";

    string type = sym->type;
    int field_offset = 0;

    if (field_offsets.count(type) && field_offsets[type].count(member))
        field_offset = field_offsets[type][member];

    string ftype = "u32";
    string key = type + "." + member;

    if (field_types.count(key))
        ftype = field_types[key];

    string sw = size_word(ftype);

    if (is_local && local_offsets.count(object)) {
        int base_offset = local_offsets[object];
        int total_offset = base_offset + field_offset;

        if (total_offset < 0)
            return sw + " [ebp - " + to_string(-total_offset) + "]";
            
        return sw + " [ebp + " + to_string(total_offset) + "]";
    }

    if (field_offset == 0)
        return sw + " [" + object + "]";

    return sw + " [" + object + " + " + to_string(field_offset) + "]";
}

string CodeGen::gen_address(ASTNode &expr) {
    if (auto *h = dynamic_cast<HexLiteral *>(&expr)) {
        stringstream ss;

        ss << "0x" << hex << h->value;

        return ss.str();
    }

    if (auto *i = dynamic_cast<IntLiteral *>(&expr))
        return to_string(i->value);

    if (auto *reg = dynamic_cast<RegOperand *>(&expr))
        return reg->name;

    if (auto *ident = dynamic_cast<Identifier *>(&expr))
        return ident->name;

    return "0";
}

string CodeGen::gen_memref_addr(MemRef &memref) {
    string addr = gen_address(*memref.address);

    if (memref.offset) {
        if (auto *bin = dynamic_cast<BinaryExpr *>(memref.offset.get())) {
            if (bin->op == "-") {
                string off = gen_operand(*bin->right);

                return (memref.segment.empty() ? "" : memref.segment + ":") + addr + " - " + off;
            }
        }

        string off = gen_operand(*memref.offset);

        return (memref.segment.empty() ? "" : memref.segment + ":") + addr + " + " + off;
    }

    if (!memref.segment.empty())
        return memref.segment + ":" + addr;

    return addr;
}

string CodeGen::size_reg(const string &reg, const string &type) {
    int size = type_size(type);

    static const map<string, array<string, 3>> reg_map = {
        {"eax", {"al",  "ax",  "eax"}},
        {"ebx", {"bl",  "bx",  "ebx"}},
        {"ecx", {"cl",  "cx",  "ecx"}},
        {"edx", {"dl",  "dx",  "edx"}},
        {"esi", {"",    "si",  "esi"}},
        {"edi", {"",    "di",  "edi"}},
        {"esp", {"",    "sp",  "esp"}},
        {"ebp", {"",    "bp",  "ebp"}},
    };

    static const map<string, string> to_base = {
        {"al", "eax"}, {"ah", "eax"}, {"ax", "eax"}, {"eax", "eax"},
        {"bl", "ebx"}, {"bh", "ebx"}, {"bx", "ebx"}, {"ebx", "ebx"},
        {"cl", "ecx"}, {"ch", "ecx"}, {"cx", "ecx"}, {"ecx", "ecx"},
        {"dl", "edx"}, {"dh", "edx"}, {"dx", "edx"}, {"edx", "edx"},
        {"si", "esi"}, {"esi", "esi"},
        {"di", "edi"}, {"edi", "edi"},
        {"sp", "esp"}, {"esp", "esp"},
        {"bp", "ebp"}, {"ebp", "ebp"},
    };

    auto base_it = to_base.find(reg);

    if (base_it == to_base.end())
        return reg;

    string base = base_it->second;
    auto map_it = reg_map.find(base);

    if (map_it == reg_map.end())
        return reg;

    if (size == 1)
        return map_it->second[0];

    if (size == 2)
        return map_it->second[1];

    return map_it->second[2];
}

void CodeGen::gen_raw_data(RawData &node) {
    out << "    " << node.directive;

    for (size_t i = 0; i < node.values.size(); i++) {
        if (i > 0)
            out << ",";

        if (auto *str = dynamic_cast<StringLiteral *>(node.values[i].get()))
            out << " \"" << str->value << "\"";
        else
            out << " " << gen_operand(*node.values[i]);
    }

    out << "\n";
}

void CodeGen::gen_fill(FillStmt &node) {
    string target = gen_operand(*node.target);
    string value = gen_operand(*node.value);

    out << "    times " << target << "-($-$$) db " << value << "\n";
}