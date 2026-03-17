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
    out << "section " << section.name << "\n";

    for (const auto &node : section.contents) {
        if (auto *var = dynamic_cast<VarDecl *>(node.get()))
            gen_var_bss(*var);
        else if (auto *cn = dynamic_cast<ConstDecl *>(node.get()))
            gen_const(*cn);
        else if (auto *conv = dynamic_cast<ConvDecl *>(node.get()))
            gen_conv(*conv);
        else if (auto *proc = dynamic_cast<ProcDecl *>(node.get()))
            gen_proc(*proc);
    }
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

void CodeGen::gen_conv(ConvDecl &decl) {
    out << "    ; conv " << decl.name << "\n";
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
    else if (auto *ex = dynamic_cast<ExitStmt *>(&node))
        gen_exit(*ex);
    else if (auto *a = dynamic_cast<AsmBlock *>(&node))
        gen_asm(*a);
    else if (dynamic_cast<RetStmt *>(&node))
        out << "    ret\n";
    else if (dynamic_cast<BreakStmt *>(&node)) {
        if (!break_label.empty())
            out << "    jmp " << break_label << "\n";
    }
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

    if (auto *bin = dynamic_cast<BinaryExpr *>(node.condition.get())) {
        string left = gen_operand(*bin->left);
        string right = gen_operand(*bin->right);

        out << "    cmp " << left << ", " << right << "\n";

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

    for (const auto &stmt : node.body)
        gen_statement(*stmt, break_label);
    
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
    string rhs = gen_operand(*node.rhs);

    out << "    mov " << lhs << ", " << rhs << "\n";
}

void CodeGen::gen_incr(IncrStmt &node) {
    auto *sym = locals.lookup(node.name);

    if (!sym)
        sym = globals.lookup(node.name);

    string op = node.is_dec ? "dec" : "inc";
    string sw = sym ? size_word(sym->type) : "dword";

    out << "    " << op << " " << sw << " [" << node.name << "]\n";
}

void CodeGen::gen_call(ProcCall &node) {
    out << "    call " << node.name << "\n";
}

void CodeGen::gen_exit(ExitStmt &node) {
    string code = gen_operand(*node.code);

    out << "    mov eax, 1\n";
    out << "    mov ebx, " << code << "\n";
    out << "    int 0x80\n";
}

void CodeGen::gen_asm(AsmBlock &node) {
    for (const auto &line : node.lines)
        out << "    " << line << "\n";
}

string CodeGen::gen_operand(ASTNode &expr) {
    if (auto *i = dynamic_cast<IntLiteral *>(&expr))
        return to_string(i->value);
    
    if (auto *h = dynamic_cast<HexLiteral *>(&expr)) {
        stringstream ss;

        ss << "0x" << hex << h->value;

        return ss.str();
    }

    if (auto *reg = dynamic_cast<RegOperand *>(&expr))
        return reg->name;
    
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

    return "0";
}

string CodeGen::gen_lhs(ASTNode &expr) {
    if (auto *reg = dynamic_cast<RegOperand *>(&expr))
        return reg->name;
    
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