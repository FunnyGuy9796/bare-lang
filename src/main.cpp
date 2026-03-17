#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/semantic.h"
#include "codegen/codegen.h"
#include "assembler/assembler.h"

using namespace std;

vector<string> input_names;
string output_name("out");

bool rec_input = false;
bool rec_output = false;

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        string str(argv[i]);

        if (str == "-b") {
            rec_input = true;

            continue;
        } else if (str == "-o") {
            rec_input = false;
            rec_output = true;
            
            continue;
        }

        if (rec_input)
            input_names.push_back(str);
        else if (rec_output)
            output_name = str;
    }
    
    if (input_names.empty()) {
        cerr << "no input files provided" << endl;

        return 1;
    }

    Lexer lexer;
    vector<token_t> tokens;
    
    for (int i = 0; i < input_names.size(); i++) {
        ifstream input(input_names[i]);

        if (!input.is_open()) {
            cerr << "could not open '" << input_names[i] << "'" << endl;

            return 1;
        }

        string line;
        uint32_t line_num = 1;

        while (getline(input, line)) {
            bool is_blank = line.find_first_not_of(" \t\r\n") == std::string::npos;

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
    string asm_name = output_name + ".asm";
    string obj_name = output_name + ".o";
    ofstream asm_file(asm_name);

    if (!asm_file.is_open()) {
        cerr << "could not create .asm file" << endl;

        return 1;
    }

    asm_file << asm_output;
    asm_file.close();

    Assembler assembler;
    string err;

    if (!assembler.assemble(asm_name, obj_name, err)) {
        cerr << "assembly failed:\n" << err << endl;

        return 1;
    }

    if (!assembler.link(obj_name, output_name, err)) {
        cerr << "linking failed:\n" << err << endl;

        return 1;
    }

    remove(asm_name.c_str());
    remove(obj_name.c_str());

    cout << "compiled successfully -> " << output_name << endl;

    return 0;
}