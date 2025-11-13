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
#include "Lexer.h"

#include <iostream>

std::unordered_map<std::string, TokenType> Lexer::keywords = {
    {"class", TokenType::KW_CLASS}, {"fn", TokenType::KW_FN}, {"let", TokenType::KW_LET},
    {"if", TokenType::KW_IF}, {"else", TokenType::KW_ELSE}, {"while", TokenType::KW_WHILE},
    {"for", TokenType::KW_FOR}, {"return", TokenType::KW_RETURN}, {"new", TokenType::KW_NEW},
    {"static", TokenType::KW_STATIC}, {"self", TokenType::KW_SELF}, {"seal", TokenType::KW_SEAL},
    {"pub", TokenType::KW_PUB}, {"priv", TokenType::KW_PRIV}, {"prot", TokenType::KW_PROT},
    {"as", TokenType::KW_AS}, {"is", TokenType::KW_IS}, {"mod", TokenType::KW_MOD},
    {"import", TokenType::KW_IMPORT}, {"use", TokenType::KW_USE}, {"break", TokenType::KW_BREAK},
    {"continue", TokenType::KW_CONTINUE}, {"op", TokenType::KW_OP}, {"const", TokenType::KW_CONST},
    {"loop", TokenType::KW_LOOP}, {"true", TokenType::BOOL}, {"false", TokenType::BOOL},
    {"null", TokenType::NULLVAL},{"in", TokenType::KW_IN}, {"err", TokenType::KW_ERR}
};

Lexer::Lexer(std::string source) : source(std::move(source)) {}

char Lexer::advance() {
    const char c = source[current++];
    if (c == '\n') { line++; column = 1; } else { column++; }
    return c;
}

char Lexer::peek() const {
    return current < source.size() ? source[current] : '\0';
}

char Lexer::peekNext() const {
    return current + 1 < source.size() ? source[current+1] : '\0';
}

bool Lexer::match(const char expected) {
    if (peek() != expected) return false;
    current++;
    column++;
    return true;
}

void Lexer::addToken(TokenType type) {
    tokens.emplace_back(type, source.substr(start, current - start), line, column);
}

void Lexer::addToken(TokenType type, const std::string& lexeme) {
    tokens.emplace_back(type, lexeme, line, column);
}

void Lexer::skipWhitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t': advance(); break;
            case '\n': advance(); break;
            case '/':
                if (peekNext() == '/') { skipComment(); break; }
                else return;
            default: return;
        }
    }
}

void Lexer::skipComment() {
    while (peek() != '\n' && peek() != '\0') advance();
}

void Lexer::number() {
    while (isdigit(peek())) advance();

    if (peek() == '.' && isdigit(peekNext())) {
        advance(); // dot
        while (isdigit(peek())) advance();
        addToken(TokenType::FLOAT);
    } else {
        addToken(TokenType::INT);
    }
}

void Lexer::string() {
    while (peek() != '"' && peek() != '\0') {
        if (peek() == '\n') line++;
        advance();
    }

    if (peek() == '"') advance(); // closing "
    const std::string extracted = source.substr(start+1, current-start-2);
    addToken(TokenType::STRING, extracted);
}

void Lexer::identifierOrKeyword() {
    while (isalnum(peek()) || peek() == '_' || peek() == '$') advance();
    const std::string text = source.substr(start, current - start);
    const auto it = keywords.find(text);
    if (it != keywords.end()) addToken(it->second);
    else addToken(TokenType::IDENTIFIER);
}

void Lexer::annotation() {
    // Collect annotation name (letters, numbers, underscores)
    std::string name;
    while (current < source.size() && (isalnum(source[current]) || source[current] == '_')) {
        name += advance();
    }

    if (name.empty()) {
        std::cerr << "Error: Empty annotation at line " << line << ", column " << column << std::endl;
        throw std::runtime_error("Expected annotation name after '@'");
    }

    static const std::unordered_map<std::string, TokenType> knownAnnotations = {
        {"ffi", TokenType::AT_FFI},
        {"deprecated", TokenType::AT_DEPRECATED},
    };

    auto it = knownAnnotations.find(name);
    if (it != knownAnnotations.end()) {
        addToken(it->second);
    } else {
        // Todo: We could later support user defined annotation as well
        std::cerr << "Error: Unknown annotation '@" << name << "' at line " << line << ", column " << column << std::endl;
        throw std::runtime_error("Unknown annotation '@" + name + "'");
    }
}

std::vector<Token> Lexer::tokenize() {
    while (current < source.size()) {
        start = current;
        skipWhitespace();
        start = current;

        if (current >= source.size()) break;

        char c = advance();

        switch (c) {
            case '+': if (match('=')) addToken(TokenType::PLUS_EQ); else addToken(TokenType::PLUS); break;
            case '-':
                if (match('=')) {
                    addToken(TokenType::MINUS_EQ);
                } else if (match('>')) {
                    addToken(TokenType::ARROW);
                } else {
                    addToken(TokenType::MINUS);
                }
                break;
            case '*': addToken(TokenType::STAR); break;
            case '/': addToken(TokenType::SLASH); break;
            case '%': addToken(TokenType::PERCENT); break;
            case '=': if (match('=')) addToken(TokenType::EQ); else addToken(TokenType::ASSIGN); break;
            case '!': if (match('=')) addToken(TokenType::NEQ); else addToken(TokenType::NOT); break;
            case '<': if (match('=')) addToken(TokenType::LTE); else addToken(TokenType::LT); break;
            case '>': if (match('=')) addToken(TokenType::GTE); else addToken(TokenType::GT); break;
            case '&': if (match('&')) addToken(TokenType::AND); break;
            case '|': if (match('|')) addToken(TokenType::OR); break;
            case '.': addToken(TokenType::DOT); break;
            case ',': addToken(TokenType::COMMA); break;
            case ':': addToken(TokenType::COLON); break;
            case ';': addToken(TokenType::SEMICOLON); break;
            case '(': addToken(TokenType::LPAREN); break;
            case ')': addToken(TokenType::RPAREN); break;
            case '{': addToken(TokenType::LBRACE); break;
            case '}': addToken(TokenType::RBRACE); break;
            case '[': addToken(TokenType::LBRACKET); break;
            case ']': addToken(TokenType::RBRACKET); break;
            case '@': annotation(); break;
            case '"': string(); break;
            default:
                if (isdigit(c)) number();
                else if (isalpha(c) || c == '_') identifierOrKeyword();
                break;
        }
    }
    addToken(TokenType::EOF_TOKEN);
    return tokens;
}