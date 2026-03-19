#include "semantic.h"

void SymbolTable::push_scope() {
    scopes.push_back({});
}

void SymbolTable::pop_scope() {
    if (!scopes.empty())
        scopes.pop_back();
}

void SymbolTable::declare(const string &name, symbol_t sym) {
    if (scopes.empty())
        throw runtime_error("no active scope to declare '" + name + "'");

    if (scopes.back().count(name))
        throw runtime_error("symbol '" + name + "' already declared in this scope");
    
    scopes.back()[name] = sym;
}

symbol_t *SymbolTable::lookup(const string &name) {
    for (int i = scopes.size() - 1; i >= 0; i--) {
        auto it = scopes[i].find(name);

        if (it != scopes[i].end())
            return &it->second;
    }

    return nullptr;
}

bool SymbolTable::has_active_scope() {
    return !scopes.empty();
}

void SemanticAnalyzer::report(const string &msg) {
    errors.push_back(msg);
}

void SemanticAnalyzer::analyze(Program &program) {
    collect_declarations(program);
    check_bodies(program);

    if (!errors.empty()) {
        for (const auto &e : errors)
            cerr << "error: " << e << endl;
        
        throw runtime_error("semantic analysis failed");
    }
}

SymbolTable &SemanticAnalyzer::get_globals() {
    return globals;
}

void SemanticAnalyzer::collect_declarations(Program &program) {
    globals.push_scope();

    for (const auto &data : program.data_decls) {
        symbol_t sym;

        sym.kind = symbol_t::Kind::DATA;
        sym.name = data->name;
        sym.type = data->name;

        try {
            globals.declare(data->name, sym);
        } catch (const runtime_error &e) {
            report(e.what());
        }
    }

    for (const auto &section : program.sections)
        collect_sections(*section);
}

void SemanticAnalyzer::collect_sections(SectionDecl &section) {
    for (const auto &node : section.contents) {
        if (auto *var = dynamic_cast<VarDecl *>(node.get())) {
            symbol_t sym;

            sym.kind = symbol_t::Kind::VAR;
            sym.name = var->name;
            sym.type = var->type;
            sym.is_array = var->is_array;
            sym.array_size = var->array_size;

            try {
                globals.declare(var->name, sym);
            } catch (const runtime_error &e) {
                report(e.what());
            }
        } else if (auto *cn = dynamic_cast<ConstDecl *>(node.get())) {
            symbol_t sym;

            sym.kind = symbol_t::Kind::CONST;
            sym.name = cn->name;
            sym.type = cn->type;

            try {
                globals.declare(cn->name, sym);
            } catch (const runtime_error &e) {
                report(e.what());
            }
        } else if (auto *proc = dynamic_cast<ProcDecl *>(node.get())) {
            symbol_t sym;

            sym.kind = symbol_t::Kind::PROC;
            sym.name = proc->name;

            try {
                globals.declare(proc->name, sym);
            } catch (const runtime_error &e) {
                report(e.what());
            }
        } else if (dynamic_cast<BitsStmt *>(node.get())) {

        } else if (dynamic_cast<OrgStmt *>(node.get())) {

        } else if (dynamic_cast<RawData *>(node.get())) {

        } else if (dynamic_cast<FillStmt *>(node.get())) {

        }
    }
}

void SemanticAnalyzer::check_bodies(Program &program) {
    for (const auto &section : program.sections) {
        for (const auto &node : section->contents) {
            if (auto *proc = dynamic_cast<ProcDecl *>(node.get()))
                check_proc(*proc);
        }
    }
}

void SemanticAnalyzer::check_proc(ProcDecl &proc) {
    locals = SymbolTable();
    in_loop = false;
    curr_proc = proc.name;

    for (const auto &stmt : proc.body)
        check_statement(*stmt);
}

void SemanticAnalyzer::check_statement(ASTNode &node) {
    if (auto *var = dynamic_cast<VarDecl *>(&node)) {
        if (!locals.has_active_scope()) {
            report("var '" + var->name + "' declared outside of frame block in proc '" + curr_proc + "'");

            return;
        }

        bool is_primitive = (var->type == "u1"  || var->type == "u8"  ||
                             var->type == "u16" || var->type == "u32" ||
                             var->type == "u64" || var->type == "i8"  ||
                             var->type == "i16" || var->type == "i32" ||
                             var->type == "i64");
        
        if (!is_primitive && !globals.lookup(var->type))
            report("unknown type '" + var->type + "' for var '" + var->name + "'");
        
        symbol_t sym;

        sym.kind = symbol_t::Kind::VAR;
        sym.name = var->name;
        sym.type = var->type;
        sym.is_array = var->is_array;
        sym.array_size = var->array_size;

        try {
            locals.declare(var->name, sym);
        } catch (const runtime_error &e) {
            report(e.what());
        }
    } else if (auto *assign = dynamic_cast<AssignStmt *>(&node))
        check_assign(*assign);
    else if (auto *call = dynamic_cast<ProcCall *>(&node)) {
        auto *sym = globals.lookup(call->name);

        if (!sym)
            report("call to undefined proc '" + call->name + "'");
        else if (sym->kind != symbol_t::Kind::PROC)
            report("'" + call->name + "' is not a proc");
    } else if (auto *incr = dynamic_cast<IncrStmt *>(&node)) {
        if (!incr->is_reg && !locals.lookup(incr->name) && !globals.lookup(incr->name))
            report("unknown variable '" + incr->name + "' in increment");
    } else if (auto *when = dynamic_cast<WhenBlock *>(&node))
        check_when(*when);
    else if (auto *loop = dynamic_cast<LoopBlock *>(&node))
        check_loop(*loop);
    else if (auto *frame = dynamic_cast<FrameBlock *>(&node))
        check_frame(*frame);
    else if (auto *out = dynamic_cast<OutStmt *>(&node)) {
        resolve_type(*out->port);
        resolve_type(*out->value);
    } else if (dynamic_cast<BreakStmt *>(&node)) {
        if (!in_loop)
            report("'break' used outside of loop in proc' " + curr_proc + "'");
    } else if (dynamic_cast<RetStmt *>(&node)) {

    } else if (dynamic_cast<AsmBlock *>(&node)) {
        
    } else if (dynamic_cast<SyscallStmt *>(&node)) {

    } else if (dynamic_cast<CliStmt *>(&node)) {

    } else if (dynamic_cast<StiStmt *>(&node)) {

    } else if (dynamic_cast<HltStmt *>(&node)) {

    } else if (dynamic_cast<MemRef *>(&node))
        report("memory reference used as statement with no assignment in proc '" + curr_proc + "'");
    else if (dynamic_cast<DerefStmt *>(&node))
        report("dereference used as statement with no assignment in proc '" + curr_proc + "'");
    else if (dynamic_cast<UnaryExpr *>(&node))
        report("unary expression used as statement with no assignment in proc '" + curr_proc + "'");
    else
        report("unknown statement in proc '" + curr_proc + "'");
}

void SemanticAnalyzer::check_assign(AssignStmt &node) {
    resolve_type(*node.rhs);

    if (auto *mem = dynamic_cast<MemberAccess *>(node.lhs.get())) {
        auto *obj = locals.lookup(mem->object);

        if (!obj)
            obj = globals.lookup(mem->object);
        
        if (!obj) {
            report("unknown variable '" + mem->object + "' in member access");

            return;
        }

        auto *data_sym = globals.lookup(obj->type);

        if (!data_sym || data_sym->kind != symbol_t::Kind::DATA)
            report("'" + obj->type + "' is not a data type, cannot access '." + mem->member + "'");
    } else if (auto *arr = dynamic_cast<ArrayAccess *>(node.lhs.get())) {
        auto *sym = locals.lookup(arr->name);

        if (!sym)
            sym = globals.lookup(arr->name);

        if (!sym)
            report("unknown variable '" + arr->name + "' in array access");
        else if (!sym->is_array)
            report("'" + arr->name + "' is not an array");
        
        resolve_type(*arr->index);
    } else if (auto *ident = dynamic_cast<Identifier *>(node.lhs.get())) {
        if (!locals.lookup(ident->name) && !globals.lookup(ident->name))
            report("assignment to undeclared variable '" + ident->name + "'");
    } else if (dynamic_cast<RegOperand *>(node.lhs.get())) {

    } else if (dynamic_cast<SegOperand *>(node.lhs.get())) {

    } else if (auto *memref = dynamic_cast<MemRef *>(node.lhs.get())) {
        if (memref->address)
            resolve_type(*memref->address);
    } else if (auto *deref = dynamic_cast<DerefStmt *>(node.lhs.get())) {
        if (auto *ident = dynamic_cast<Identifier *>(deref->ptr.get())) {
            if (!locals.lookup(ident->name) && !globals.lookup(ident->name))
                report("unknown pointer variable '" + ident->name + "' in dereference");
        }

        if (deref->offset)
            resolve_type(*deref->offset);
    }
}

string SemanticAnalyzer::resolve_type(ASTNode &expr) {
    if (dynamic_cast<IntLiteral *>(&expr))
        return "u32";
    
    if (dynamic_cast<HexLiteral *>(&expr))
        return "u32";
    
    if (dynamic_cast<RegOperand *>(&expr))
        return "u32";
    
    if (dynamic_cast<SegOperand *>(&expr))
        return "u16";
    
    if (dynamic_cast<NullLiteral *>(&expr))
        return "u32";
    
    if (auto *memref = dynamic_cast<MemRef *>(&expr)) {
        if (memref->address)
            resolve_type(*memref->address);
        
        if (memref->offset)
            resolve_type(*memref->offset);
        
        return memref->type.empty() ? "u32" : memref->type;
    }

    if (auto *deref = dynamic_cast<DerefStmt *>(&expr)) {
        if (auto *ident = dynamic_cast<Identifier *>(deref->ptr.get())) {
            auto *sym = locals.lookup(ident->name);

            if (!sym)
                sym = globals.lookup(ident->name);
            
            if (!sym) {
                report("use of undeclared identifier '" + ident->name + "'");

                return "unknown";
            }
        } else if (dynamic_cast<RegOperand *>(deref->ptr.get())) {

        }

        if (deref->offset)
            resolve_type(*deref->offset);
        
        return deref->type.empty() ? "u32" : deref->type;
    }

    if (auto *addr = dynamic_cast<AddrOf *>(&expr)) {
        auto *sym = locals.lookup(addr->name);

        if (!sym)
            sym = globals.lookup(addr->name);

        if (!sym)
            report("unknown variable '" + addr->name + "' in addr expression");
            
        return "u32";
    }
    
    if (auto *ident = dynamic_cast<Identifier *>(&expr)) {
        auto *sym = locals.lookup(ident->name);

        if (!sym)
            sym = globals.lookup(ident->name);
        
        if (!sym) {
            report("use of undeclared identifier '" + ident->name + "'");

            return "unknown";
        }

        return sym->type;
    }

    if (auto *mem = dynamic_cast<MemberAccess *>(&expr)) {
        auto *obj = locals.lookup(mem->object);

        if (!obj)
            obj = globals.lookup(mem->object);
        
        if (!obj) {
            report("unknown variable '" + mem->object + "'");

            return "unknown";
        }

        return obj->type;
    }

    if (auto *arr = dynamic_cast<ArrayAccess *>(&expr)) {
        auto *sym = locals.lookup(arr->name);

        if (!sym)
            sym = globals.lookup(arr->name);
        
        if (!sym) {
            report("unknown array '" + arr->name + "'");

            return "unknown";
        }

        return sym->type;
    }

    if (auto *bin = dynamic_cast<BinaryExpr *>(&expr)) {
        resolve_type(*bin->left);
        resolve_type(*bin->right);

        if (bin->op == ">" || bin->op == "<" || bin->op == "==" || bin->op == ">=" || bin->op == "<=" || bin->op == "!=")
            return "u1";
        
        return resolve_type(*bin->left);
    }

    if (auto *unary = dynamic_cast<UnaryExpr *>(&expr))
        return resolve_type(*unary->operand);
    
    if (auto *in = dynamic_cast<InStmt *>(&expr)) {
        resolve_type(*in->port);

        return "u32";
    }

    if (auto *cast = dynamic_cast<CastExpr *>(&expr)) {
        resolve_type(*cast->expr);

        return cast->type;
    }

    if (auto *sz = dynamic_cast<SizeofExpr *>(&expr))
        return "u32";

    report("unresolved expression type");

    return "unknown";
}

void SemanticAnalyzer::check_when(WhenBlock &node) {
    string cond_type = resolve_type(*node.condition);

    if (cond_type != "u1" && cond_type != "unknown")
        report("'when' condition must be a comparison in proc '" + curr_proc + "'");

    for (const auto &stmt : node.body)
        check_statement(*stmt);
    
    for (const auto &stmt : node.else_body)
        check_statement(*stmt);
}

void SemanticAnalyzer::check_loop(LoopBlock &node) {
    bool prev = in_loop;

    in_loop = true;

    for (const auto &stmt : node.body)
        check_statement(*stmt);

    in_loop = prev;
}

void SemanticAnalyzer::check_frame(FrameBlock &node) {
    locals.push_scope();

    for (const auto &stmt : node.body)
        check_statement(*stmt);

    locals.pop_scope();
}