#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <vector>

using namespace std;

class Assembler {
public:
    static int run(const vector<string> &args, string &err_output);
    static bool assemble(const string &asm_file, const string &obj_file, string &err);
    static bool link(const string &obj_file, const string &out_file, string &err);
};

#endif