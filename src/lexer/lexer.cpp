#include "lexer.h"

static const vector<string> KEYWORDS = {
    "data", "end", "section", "const", "var", "proc", "when", "ret", "reg", "seg",
    "frame", "loop", "break", "args", "null", "asm", "syscall", "addr", "in", "out",
    "cli", "sti", "hlt", "bits", "db", "dw", "dd", "dq", "fill"
};

static const vector<string> TYPES = {
    "u1", "u8", "u16", "u32", "u64", "i8", "i16", "i32", "i64"
};

static const vector<string> REG_NAMES = {
    "eax", "ebx", "ecx", "edx", "esi", "edi", "esp", "ebp",
    "ax", "bx", "cx", "dx", "si", "di", "sp", "bp",
    "al", "ah", "bl", "bh", "cl", "ch", "dl", "dh"
};

static const vector<string> SEG_NAMES = {
    "cs", "ds", "ss", "es", "fs", "gs"
};

static const vector<string> OPERATORS = {
    ">", "<", ">=", "<=", "==", "!=", "+", "-", "++", "--",
    "|", "^", "<<", ">>", "~"
};

token_type_t Lexer::classify(const string &value) {
    for (const auto &kw : KEYWORDS) {
        if (value == kw)
            return KEYWORD;
    }

    for (const auto &t : TYPES) {
        if (value == t)
            return TYPE;
    }

    for (const auto &r : REG_NAMES) {
        if (value == r)
            return REG_NAME;
    }

    for (const auto &s : SEG_NAMES) {
        if (value == s)
            return SEG_NAME;
    }

    for (const auto &o : OPERATORS) {
        if (value == o)
            return OPERATOR;
    }

    if (value == "=")
        return ASSIGN;
    
    if (value == ":")
        return COLON;
    
    if (value == ".")
        return DOT;
    
    if (value == ",")
        return COMMA;
    
    if (value == "&")
        return AMPERSAND;
    
    if (value == "*")
        return STAR;
    
    if (value == "(")
        return LPAREN;
    
    if (value == ")")
        return RPAREN;
    
    if (value == "[")
        return LBRACK;
    
    if (value == "]")
        return RBRACK;
    
    if (value[0] == '.')
        return SECTION_NAME;
    
    if (value.size() > 2 && value[0] == '0' && value[1] == 'x')
        return HEX_LITERAL;
    
    bool is_int = true;

    for (char c : value) {
        if (!isdigit(c)) {
            is_int = false;

            break;
        }
    }

    if (is_int && !value.empty())
        return INT_LITERAL;
    
    return IDENT;
}

static vector<string> split_lexum(const string &raw) {
    vector<string> parts;
    string curr = "";

    for (size_t i = 0; i < raw.size(); i++) {
        char c = raw[i];

        if (c == '(' || c == ')' || c == '[' || c == ']' || c == ':') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";
            }

            parts.push_back(string(1, c));
        } else if (c == '.') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";

                parts.push_back(".");
            } else
                curr.push_back(c);
        } else if (c == ',') {
            if (!curr.empty()) {
                parts.push_back(curr);
                
                curr = "";
            }
            
            parts.push_back(",");
        } else if (c == '&') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";
            }

            parts.push_back("&");
        } else if (c == '*') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";
            }

            parts.push_back("*");
        } else if ((c == '+' || c == '-' || c == '=') && i + 1 < raw.size() && raw[i + 1] == c) {
            if (!curr.empty()) {
                parts.push_back(curr);
                
                curr = "";
            }

            parts.push_back(string(2, c));

            i++;
        } else if (c == '<' && i + 1 < raw.size() && raw[i + 1] == '<') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";
            }

            parts.push_back("<<");

            i++;
        } else if (c == '>' && i + 1 < raw.size() && raw[i + 1] == '>') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";
            }

            parts.push_back(">>");

            i++;
        } else if (c == '|' || c == '^' || c == '~') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";
            }

            parts.push_back(string(1, c));
        } else if (c == '>' || c == '<') {
            if (!curr.empty()) {
                parts.push_back(curr);

                curr = "";
            }

            if (i + 1 < raw.size() && raw[i + 1] == '=') {
                parts.push_back(string(1, c) + "=");
                
                i++;
            } else
                parts.push_back(string(1, c));
        } else
            curr.push_back(c);
    }

    if (!curr.empty())
        parts.push_back(curr);
    
    return parts;
}

vector<token_t> Lexer::lex_file(map<uint32_t, string> &content) {
    string curr_token = "";
    vector<token_t> tokens;

    for (const auto &[line_num, line_content] : content) {
        size_t first = line_content.find_first_not_of(" \t");

        if (first != string::npos && line_content.substr(first, 2) == "//")
            continue;
        
        for (size_t i = 0; i < line_content.size(); i++) {
            char c = line_content[i];

            if (c == '/' && i + 1 < line_content.size() && line_content[i + 1] == '/') {
                if (!curr_token.empty()) {
                    for (const auto &part : split_lexum(curr_token))
                        tokens.push_back({ classify(part), part, line_num });
                    
                    curr_token.clear();
                }

                break;
            }

            if (c == '\'') {
                if (!curr_token.empty()) {
                    for (const auto &part : split_lexum(curr_token))
                        tokens.push_back({ classify(part), part, line_num });

                    curr_token.clear();
                }

                i++;

                uint8_t val = 0;

                if (i < line_content.size() && line_content[i] != '\'') {
                    char ch = line_content[i];

                    val = (uint8_t)ch;

                    if (ch == '\\' && i + 1 < line_content.size()) {
                        i++;

                        switch (line_content[i]) {
                            case 'n': {
                                val = '\n';
                                
                                break;
                            }

                            case 't': {
                                val = '\t';
                                
                                break;
                            }

                            case 'r': {
                                val = '\r';
                                
                                break;
                            }

                            case '0': {
                                val = '\0';
                                
                                break;
                            }

                            case '\\': {
                                val = '\\';
                                
                                break;
                            }
                            case '\'': {
                                val = '\'';
                                
                                break;
                            }

                            default: {
                                val = line_content[i];
                                
                                break;
                            }
                        }
                    }

                    i++;
                }

                tokens.push_back({ INT_LITERAL, to_string(val), line_num });
                
                continue;
            }

            if (c == '"') {
                if (!curr_token.empty()) {
                    for (const auto &part : split_lexum(curr_token))
                        tokens.push_back({ classify(part), part, line_num });

                    curr_token.clear();
                }

                string str_val = "";

                i++;

                while (i < line_content.size() && line_content[i] != '"') {
                    str_val += line_content[i];
                    i++;
                }

                tokens.push_back({ STRING_LITERAL, str_val, line_num });

                continue;
            }

            if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
                if (!curr_token.empty()) {
                    for (const auto &part : split_lexum(curr_token))
                        tokens.push_back({ classify(part), part, line_num });

                    curr_token.clear();
                }

                continue;
            }

            curr_token.push_back(c);
        }

        if (!curr_token.empty()) {
            for (const auto &part : split_lexum(curr_token))
                tokens.push_back({ classify(part), part, line_num });
            
            curr_token.clear();
        }
    }

    return tokens;
}