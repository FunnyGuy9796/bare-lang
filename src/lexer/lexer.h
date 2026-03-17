#ifndef LEXER_H
#define LEXER_H

#include <iostream>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

using namespace std;

typedef enum {
    KEYWORD,
    IDENT,
    TYPE,
    ARROW,
    COLON,
    DOT,
    LPAREN,
    RPAREN,
    LBRACK,
    RBRACK,
    OPERATOR,
    REG_NAME,
    ASSIGN,
    SECTION_NAME,
    INT_LITERAL,
    HEX_LITERAL,
    STRING_LITERAL
} token_type_t;

typedef struct {
    token_type_t type;
    string value;
    uint32_t line;
} token_t;

class Lexer {
public:
    map<string, map<uint32_t, string>> files;

    vector<token_t> lex_file(map<uint32_t, string> &content);

private:
    token_type_t classify(const string &value);
};

#endif