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
 *  File: Parser
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_PARSER_H
#define DROPLET_PARSER_H
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Lexer.h"
#include "Program.h"
#include "Stmt.h"

class ParseError final : public std::runtime_error {
public:
    ParseError(const std::string& msg, const int line, const int col)
        : std::runtime_error(formatError(msg, line, col)),
          line_(line), column_(col) {}

    int line() const { return line_; }
    int column() const { return column_; }

private:
    int line_;
    int column_;

    static std::string formatError(const std::string& msg, const int line, const int col) {
        std::ostringstream oss;
        oss << "Parse error at line " << line << ", column " << col << ": " << msg;
        return oss.str();
    }
};


class Parser {
    public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

private:
    std::vector<Token> tokens;
    size_t current = 0;

    // Utility methods
    Token peek() const;
    Token previous() const;
    Token advance();
    bool check(TokenType type) const;
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    bool isAtEnd() const;
    ParseError error(const std::string& message) const;
    void synchronize();

    // Top-level parsing
    std::unique_ptr<ModuleDecl> parseModuleDecl();
    std::unique_ptr<ImportStmt> parseImportStmt();
    std::unique_ptr<ClassDecl> parseClassDecl();
    std::unique_ptr<FunctionDecl> parseFunctionDecl(bool isMethod = false);
    std::unique_ptr<FunctionDecl> parseFFIDecl();

    // Class member parsing
    FieldDecl parseFieldDecl(FieldDecl::Visibility vis, bool isStatic);
    std::unique_ptr<FunctionDecl> parseMethodDecl(FieldDecl::Visibility vis, bool isStatic, bool isSealed);
    std::unique_ptr<FunctionDecl> parseConstructor();
    std::unique_ptr<FunctionDecl> parseOperatorOverload(FieldDecl::Visibility vis);

    // Statement parsing
    StmtPtr parseStatement();
    StmtPtr parseVarDecl();
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForStmt();
    StmtPtr parseLoopStmt();
    StmtPtr parseReturnStmt();
    StmtPtr parseBreakStmt();
    StmtPtr parseContinueStmt();
    StmtPtr parseBlock();
    StmtPtr parseExprStmt();

    // Expression parsing
    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();

    // Helper expression parsers
    ExprPtr parseCall(ExprPtr callee);
    ExprPtr parseFieldAccess(ExprPtr object);
    ExprPtr parseIndex(ExprPtr object);
    ExprPtr parseNewExpr();
    ExprPtr parseListLiteral();
    ExprPtr parseDictLiteral() const;

    // Type parsing
    std::string parseType();
    std::vector<std::string> parseTypeParams();

    // Helper methods
    std::vector<Parameter> parseParameters();
    std::vector<ExprPtr> parseArguments();
    FieldDecl::Visibility parseVisibility();
    std::string parseQualifiedName();
};


#endif //DROPLET_PARSER_H