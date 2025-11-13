/*
 * ============================================================
 *  Droplet 
 * ============================================================
 *  Copyright (c) 2025 Droplet Contributors
 *  All rights reserved.
 *
 *  Licensed under the MIT License.
 *  See LICENSE file in the project root for full license.
 *
 *  File: Lexer
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_LEXER_H
#define DROPLET_LEXER_H
#include <string>
#include <unordered_map>
#include <vector>

enum class TokenType {
    // Literals
    INT, FLOAT, BOOL, STRING, NULLVAL,

    // Identifiers
    IDENTIFIER,

    // Keywords
    KW_CLASS, KW_FN, KW_LET, KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_RETURN,
    KW_NEW, KW_STATIC, KW_SELF, KW_SEAL, KW_PUB, KW_PRIV, KW_PROT,
    KW_AS, KW_IS, KW_MOD, KW_IMPORT, KW_USE, KW_BREAK, KW_CONTINUE,
    KW_OP, KW_CONST, KW_LOOP, KW_IN,

    // error
    KW_ERR,

    // Operators & punctuation
    PLUS, MINUS, STAR, SLASH, PERCENT, ASSIGN, PLUS_EQ, MINUS_EQ,
    EQ, NEQ, LT, LTE, GT, GTE,
    AND, OR, NOT, ARROW,
    DOT, COMMA, COLON, SEMICOLON,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,

    // Annotations
    AT_FFI, AT_DEPRECATED,

    // End of file
    EOF_TOKEN,
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    Token(const TokenType t, const std::string& l, const int ln, const int col)
        : type(t), lexeme(l), line(ln), column(col) {}
};

class Lexer {
public:
    explicit Lexer(std::string  source);

    std::vector<Token> tokenize();

private:
    std::string source;
    size_t start = 0;
    size_t current = 0;
    int line = 1;
    int column = 1;

    std::vector<Token> tokens;

    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);

    void addToken(TokenType type);
    void addToken(TokenType type, const std::string& lexeme);

    void skipWhitespace();
    void skipComment();

    void number();
    void string();
    void identifierOrKeyword();
    void annotation();

    static std::unordered_map<std::string, TokenType> keywords;
};


#endif //DROPLET_LEXER_H