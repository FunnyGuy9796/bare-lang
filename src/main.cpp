#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "codegen/codegen.h"
#include "assembler/assembler.h"

using namespace std;

vector<string> input_names;
string output_name = "";

bool rec_input = false;
bool rec_output = false;
bool emit_asm = false;
bool emit_bin = false;

void print_help() {
    cout << "Usage: bare-compiler -b <files...> [-o <output>] [options]\n\n";
    cout << "Input:\n";
    cout << "  -b <files...>     specify input .bare source files\n\n";
    cout << "Output:\n";
    cout << "  -o <output>       specify output file name (default: out.o)\n";
    cout << "  -S                emit assembly only (.asm)\n";
    cout << "  -bin              assemble to flat binary (for bootloaders)\n\n";
    cout << "Options:\n";
    cout << "  -h, --help        show this help message\n";
    cout << "  -v, --version     show compiler version\n\n";
    cout << "Examples:\n";
    cout << "  bare-compiler -b main.bare -o main.o\n";
    cout << "  bare-compiler -b main.bare -S -o main.asm\n";
    cout << "  bare-compiler -b boot.bare -o boot.bin -bin\n\n";
    cout << "Linking examples:\n";
    cout << "  Linux ELF32:\n";
    cout << "    ld -m elf_i386 -o program main.o\n\n";
    cout << "  Bootloader (manual):\n";
    cout << "    ld -m elf_i386 --oformat binary -Ttext 0x7C00 -o boot.bin boot.o\n\n";
    cout << "  With GCC:\n";
    cout << "    gcc -m32 -nostdlib -o program main.o\n";
}

void print_version() {
    cout << "bare-compiler 0.1.0\n";
}

string preprocess(const string &filepath, vector<string> &visited) {
    for (const auto &v : visited) {
        if (v == filepath) {
            cerr << "warning: circular include '" << filepath << "'" << endl;

            return "";
        }
    }

    visited.push_back(filepath);

    ifstream file(filepath);

    if (!file.is_open())
        throw runtime_error("could not open '" + filepath + "'");
    
    string result;
    string line;

    while (getline(file, line)) {
        size_t first = line.find_first_not_of(" \t");

        if (first != string::npos && line.substr(first, 9) == "#include ") {
            size_t q1 = line.find('"', first + 9);
            size_t q2 = line.find('"', q1 + 1);

            if (q1 == string::npos || q2 == string::npos)
                throw runtime_error("malformed #include in '" + filepath + "'");

            string inc_file = line.substr(q1 + 1, q2 - q1 - 1);
            filesystem::path base = filesystem::path(filepath).parent_path();
            string inc_path = (base / inc_file).string();

            result += preprocess(inc_path, visited);
        } else
            result += line + "\n";
    }

    visited.pop_back();

    return result;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        string str(argv[i]);

        if (str == "-h" || str == "--help") {
            print_help();
            
            return 0;
        } else if (str == "-v" || str == "--version") {
            print_version();

            return 0;
        } else if (str == "-b") {
            rec_input = true;
            rec_output = false;

            continue;
        } else if (str == "-o") {
            rec_input = false;
            rec_output = true;

            continue;
        } else if (str == "-S") {
            emit_asm = true;
            rec_output = false;

            continue;
        } else if (str == "-bin") {
            emit_bin = true;
            rec_input = false;
            rec_output = false;

            continue;
        }

        if (rec_input)
            input_names.push_back(str);
        else if (rec_output)
            output_name = str;
    }

    if (argc == 1) {
        print_help();

        return 0;
    }

    if (output_name.empty()) {
        if (emit_asm)
            output_name = "out.asm";
        else if (emit_bin)
            output_name = "out.bin";
        else
            output_name = "out.o";
    }

    if (input_names.empty()) {
        cerr << "no input files provided" << endl;

        return 1;
    }

    Lexer lexer;
    vector<token_t> tokens;

    for (size_t i = 0; i < input_names.size(); i++) {
        string expanded;
        vector<string> visited;

        try {
            expanded = preprocess(input_names[i], visited);
        } catch (const runtime_error &e) {
            cerr << e.what() << endl;

            return 1;
        }

        istringstream stream(expanded);
        string line;
        uint32_t line_num = 1;

        while (getline(stream, line)) {
            bool is_blank = line.find_first_not_of(" \t\r\n") == string::npos;

            if (line.empty() || is_blank) {
                line_num++;

                continue;
            }

            lexer.files[input_names[i]][line_num] = line;
            line_num++;
        }

        vector<token_t> file_toks = lexer.lex_file(lexer.files[input_names[i]]);

        tokens.insert(tokens.end(), file_toks.begin(), file_toks.end());
    }

    Parser parser(tokens);
    unique_ptr<Program> program = parser.parse();
    SemanticAnalyzer semantic;

    semantic.analyze(*program);

    CodeGen code_gen(semantic.get_globals());
    string asm_output = code_gen.generate(*program);

    if (emit_asm) {
        ofstream asm_file(output_name);

        if (!asm_file.is_open()) {
            cerr << "could not create '" << output_name << "'" << endl;

            return 1;
        }

        asm_file << asm_output;
        cout << "compiled successfully -> " << output_name << endl;

        return 0;
    }

    string tmp_asm = output_name + ".tmp.asm";
    ofstream asm_file(tmp_asm);

    if (!asm_file.is_open()) {
        cerr << "could not create temp .asm file" << endl;

        return 1;
    }

    asm_file << asm_output;
    asm_file.close();

    Assembler assembler;
    string err;

    if (emit_bin) {
        if (!assembler.assemble_bin(tmp_asm, output_name, err)) {
            cerr << "assembly failed:\n" << err << endl;

            remove(tmp_asm.c_str());

            return 1;
        }

        remove(tmp_asm.c_str());

        cout << "compiled successfully -> " << output_name << endl;

        return 0;
    }

    if (!assembler.assemble(tmp_asm, output_name, err)) {
        cerr << "assembly failed:\n" << err << endl;

        remove(tmp_asm.c_str());

        return 1;
    }

    remove(tmp_asm.c_str());

    cout << "compiled successfully -> " << output_name << endl;

    return 0;
}