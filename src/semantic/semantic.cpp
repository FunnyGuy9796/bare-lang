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
        } else if (auto *conv = dynamic_cast<ConvDecl *>(node.get())) {
            symbol_t sym;

            sym.kind = symbol_t::Kind::CONV;
            sym.name = conv->name;

            try {
                globals.declare(conv->name, sym);
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

    if (!proc.calling_conv.empty()) {
        auto *sym = globals.lookup(proc.calling_conv);

        if (!sym)
            report("proc '" + proc.name + "' uses unknown conv '" + proc.calling_conv + "'");
        else if (sym->kind != symbol_t::Kind::CONV)
            report("'" + proc.calling_conv + "' is not a conv");
    }

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
        if (!locals.lookup(incr->name) && !globals.lookup(incr->name))
            report("unkown variable '" + incr->name + "' in increment");
    } else if (auto *when = dynamic_cast<WhenBlock *>(&node))
        check_when(*when);
    else if (auto *loop = dynamic_cast<LoopBlock *>(&node))
        check_loop(*loop);
    else if (auto *frame = dynamic_cast<FrameBlock *>(&node))
        check_frame(*frame);
    else if (dynamic_cast<BreakStmt *>(&node)) {
        if (!in_loop)
            report("'break' used outside of loop in proc' " + curr_proc + "'");
    } else if (auto *ex = dynamic_cast<ExitStmt *>(&node))
        resolve_type(*ex->code);
    else if (dynamic_cast<RetStmt *>(&node)) {

    } else if (dynamic_cast<AsmBlock *>(&node)) {
        
    } else
        report("unkown statement in proc '" + curr_proc + "'");
}

void SemanticAnalyzer::check_assign(AssignStmt &node) {
    resolve_type(*node.rhs);

    if (auto *mem = dynamic_cast<MemberAccess *>(node.lhs.get())) {
        auto *obj = locals.lookup(mem->object);

        if (!obj)
            obj = globals.lookup(mem->object);
        
        if (!obj) {
            report("unkown variable '" + mem->object + "' in member access");

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
            report("unkown variable '" + arr->name + "' in array access");
        else if (!sym->is_array)
            report("'" + arr->name + "' is not an array");
        
        resolve_type(*arr->index);
    } else if (auto *ident = dynamic_cast<Identifier *>(node.lhs.get())) {
        if (!locals.lookup(ident->name) && !globals.lookup(ident->name))
            report("assignment to undeclared variable '" + ident->name + "'");
    } else if (dynamic_cast<RegOperand *>(node.lhs.get())) {

    }
}

string SemanticAnalyzer::resolve_type(ASTNode &expr) {
    if (dynamic_cast<IntLiteral *>(&expr))
        return "u32";
    
    if (dynamic_cast<HexLiteral *>(&expr))
        return "u32";
    
    if (dynamic_cast<RegOperand *>(&expr))
        return "u32";
    
    if (auto *ident = dynamic_cast<Identifier *>(&expr)) {
        auto *sym = locals.lookup(ident->name);

        if (!sym)
            sym = globals.lookup(ident->name);
        
        if (!sym) {
            report("use of undeclared identifier '" + ident->name + "'");

            return "unkown";
        }

        return sym->type;
    }

    if (auto *mem = dynamic_cast<MemberAccess *>(&expr)) {
        auto *obj = locals.lookup(mem->object);

        if (!obj)
            obj = globals.lookup(mem->object);
        
        if (!obj) {
            report("unkown variable '" + mem->object + "'");

            return "unkown";
        }

        return obj->type;
    }

    if (auto *arr = dynamic_cast<ArrayAccess *>(&expr)) {
        auto *sym = locals.lookup(arr->name);

        if (!sym)
            sym = globals.lookup(arr->name);
        
        if (!sym) {
            report("unkown array '" + arr->name + "'");

            return "unkown";
        }

        return sym->type;
    }

    if (auto *bin = dynamic_cast<BinaryExpr *>(&expr)) {
        resolve_type(*bin->left);
        resolve_type(*bin->right);

        if (bin->op == "==" || bin->op == ">=" || bin->op == "<=" || bin->op == "!=")
            return "u1";
        
        return resolve_type(*bin->left);
    }

    report("unresolved expression type");

    return "unkown";
}

void SemanticAnalyzer::check_when(WhenBlock &node) {
    string cond_type = resolve_type(*node.condition);

    if (cond_type != "u1" && cond_type != "unknown")
        report("'when' condition must be a comparison in proc '" + curr_proc + "'");

    for (const auto &stmt : node.body)
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