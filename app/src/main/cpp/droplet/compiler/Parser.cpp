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
#include "Parser.h"

#include <iostream>

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

Program Parser::parse() {
    Program program;

    try {
        // Parse module declaration if present
        if (check(TokenType::KW_MOD)) {
            program.moduleDecl = parseModuleDecl();
        }

        // Parse imports
        while (check(TokenType::KW_IMPORT) || check(TokenType::KW_USE)) {
            program.imports.push_back(parseImportStmt());
        }

        // Parse top-level declarations
        while (!isAtEnd()) {
            // Check for @ffi
            if (check(TokenType::AT_FFI)) {
                program.functions.push_back(parseFFIDecl());
            } else if (check(TokenType::KW_CLASS) || check(TokenType::KW_SEAL)) {
                program.classes.push_back(parseClassDecl());
            } else if (check(TokenType::KW_FN)) {
                program.functions.push_back(parseFunctionDecl(false));
            } else {
                throw error("Expected class, function, or FFI declaration");
            }
        }
    } catch (const ParseError& e) {
        std::cerr << e.what() << std::endl;
        throw;
    }

    return program;
}

Token Parser::peek() const {
    return tokens[current];
}

Token Parser::previous() const {
    return tokens[current - 1];
}

Token Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::check(const TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(const std::initializer_list<TokenType> types) {
    for (const TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(const TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw error(message);
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::EOF_TOKEN;
}

ParseError Parser::error(const std::string& message) const {
    const Token token = peek();
    return ParseError(message, token.line, token.column);
}

void Parser::synchronize() {
    advance();
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) return;
        switch (peek().type) {
            case TokenType::KW_CLASS:
            case TokenType::KW_FN:
            case TokenType::KW_LET:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_FOR:
            case TokenType::KW_RETURN:
                return;
            default:
                advance();
        }
    }
}

std::unique_ptr<ModuleDecl> Parser::parseModuleDecl() {
    consume(TokenType::KW_MOD, "Expected 'mod'");
    std::string moduleName = parseQualifiedName();
    return std::make_unique<ModuleDecl>(moduleName);
}

std::unique_ptr<ImportStmt> Parser::parseImportStmt() {
    advance(); // consume 'import' or 'use'

    std::string modulePath = parseQualifiedName();
    std::vector<std::string> symbols;
    bool isWildcard = false;

    // Check for specific symbol imports: import std.math { sin, cos, tan }
    if (match({TokenType::LBRACE})) {
        do {
            if (match({TokenType::STAR})) {
                isWildcard = true;
                break;
            }
            Token symbol = consume(TokenType::IDENTIFIER, "Expected symbol name");
            symbols.push_back(symbol.lexeme);
        } while (match({TokenType::COMMA}));

        consume(TokenType::RBRACE, "Expected '}' after import symbols");
    } else {
        // Default: import everything
        isWildcard = true;
    }

    return std::make_unique<ImportStmt>(modulePath, symbols, isWildcard);
}

std::unique_ptr<ClassDecl> Parser::parseClassDecl() {
    bool isSealed = false;
    if (match({TokenType::KW_SEAL})) {
        isSealed = true;
    }

    consume(TokenType::KW_CLASS, "Expected 'class'");

    Token className = consume(TokenType::IDENTIFIER, "Expected class name");

    // Parse type parameters for generics
    std::vector<std::string> typeParams = parseTypeParams();

    // Parse parent class
    std::string parentClass;
    if (match({TokenType::COLON})) {
        const Token parent = consume(TokenType::IDENTIFIER, "Expected parent class name");
        parentClass = parent.lexeme;
    }

    consume(TokenType::LBRACE, "Expected '{' after class header");

    auto classDecl = std::make_unique<ClassDecl>(className.lexeme, typeParams, parentClass, isSealed);

    // Parse class body
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        // Parse visibility modifiers
        const FieldDecl::Visibility vis = parseVisibility();

        // Check for static
        bool isStatic = false;
        if (match({TokenType::KW_STATIC})) {
            isStatic = true;
        }

        // Check for sealed methods
        bool isSealedMember = false;
        if (match({TokenType::KW_SEAL})) {
            isSealedMember = true;
        }

        // Parse member
        if (check(TokenType::KW_NEW)) {
            classDecl->constructor = parseConstructor();
        } else if (check(TokenType::KW_OP)) {
            classDecl->methods.push_back(parseOperatorOverload(vis));
        } else if (check(TokenType::KW_FN)) {
            classDecl->methods.push_back(parseMethodDecl(vis, isStatic, isSealedMember));
        } else {
            classDecl->fields.push_back(parseFieldDecl(vis, isStatic));
        }
    }

    consume(TokenType::RBRACE, "Expected '}' after class body");

    return classDecl;
}

std::unique_ptr<FunctionDecl> Parser::parseFunctionDecl(bool isMethod) {
    consume(TokenType::KW_FN, "Expected 'fn'");

    Token funcName = consume(TokenType::IDENTIFIER, "Expected function name");

    consume(TokenType::LPAREN, "Expected '(' after function name");
    std::vector<Parameter> params = parseParameters();
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    std::string returnType;

    if (check(TokenType::ARROW)) {
        advance();
        returnType = parseType();
    } else if (check(TokenType::MINUS)) {
        // MINUS + GT case, we can handle arrow and this both
        advance();
        if (check(TokenType::GT)) {
            advance();
            returnType = parseType();
        } else {
            throw error("Expected '>' after '-' in return type");
        }
    }

    // check if there may be error
    // T?
    bool mayError = false;
    if (check(TokenType::NOT)) {
        consume(TokenType::NOT, "Error");
        mayError = true;
    }

    StmtPtr body = parseBlock();

    auto func = std::make_unique<FunctionDecl>(funcName.lexeme, params, returnType, std::move(body));
    func->mayReturnError = mayError;
    return func;
}

std::unique_ptr<FunctionDecl> Parser::parseFFIDecl() {
    // Consume the @ffi token
    consume(TokenType::AT_FFI, "Expected '@ffi'");

    consume(TokenType::LPAREN, "Expected '(' after @ffi");

    // Parse library name
    Token libToken = consume(TokenType::STRING, "Expected library name");
    std::string libName = libToken.lexeme;

    std::string signature;

    // Parse optional parameters (sig="...")
    while (match({TokenType::COMMA})) {
        Token key = consume(TokenType::IDENTIFIER, "Expected parameter name");
        consume(TokenType::ASSIGN, "Expected '=' after parameter name");
        const Token value = consume(TokenType::STRING, "Expected string value");

        if (key.lexeme == "sig") {
            signature = value.lexeme;
        }
    }

    consume(TokenType::RPAREN, "Expected ')' after FFI parameters");

    // Parse function signature
    consume(TokenType::KW_FN, "Expected 'fn' after FFI declaration");
    Token nameToken = consume(TokenType::IDENTIFIER, "Expected function name");

    consume(TokenType::LPAREN, "Expected '(' after function name");
    std::vector<Parameter> params = parseParameters();
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    std::string returnType;
    if (match({TokenType::ARROW})) {
        returnType = parseType();
    }

    // FFI functions usually have no body
    StmtPtr body = nullptr;

    // Construct FunctionDecl with FFI info
    auto func = std::make_unique<FunctionDecl>(
        nameToken.lexeme, params, returnType, std::move(body)
    );

    func->ffi = FFIInfo{libName, signature};

    return func;
}

FieldDecl Parser::parseFieldDecl(FieldDecl::Visibility vis, bool isStatic) {
    const Token fieldName = consume(TokenType::IDENTIFIER, "Expected field name");
    consume(TokenType::COLON, "Expected ':' after field name");
    std::string type = parseType();

    ExprPtr initializer = nullptr;
    if (match({TokenType::ASSIGN})) {
        initializer = parseExpression();
    }

    return FieldDecl(fieldName.lexeme, type, std::move(initializer), isStatic, vis);
}

std::unique_ptr<FunctionDecl> Parser::parseMethodDecl(const FieldDecl::Visibility vis, const bool isStatic, const bool isSealed) {
    auto method = parseFunctionDecl(true);
    method->isStatic = isStatic;
    method->isSealed = isSealed;
    switch (vis) {
        case FieldDecl::Visibility::PUBLIC:
            method->visibility = FunctionDecl::Visibility::PUBLIC;
            break;
        case FieldDecl::Visibility::PRIVATE:
            method->visibility = FunctionDecl::Visibility::PRIVATE;
            break;
        case FieldDecl::Visibility::PROTECTED:
            method->visibility = FunctionDecl::Visibility::PROTECTED;
            break;
    }
    return method;
}

std::unique_ptr<FunctionDecl> Parser::parseConstructor() {
    consume(TokenType::KW_NEW, "Expected 'new'");

    consume(TokenType::LPAREN, "Expected '(' after 'new'");
    std::vector<Parameter> params = parseParameters();
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    StmtPtr body = parseBlock();

    return std::make_unique<FunctionDecl>("new", params, "", std::move(body));
}

std::unique_ptr<FunctionDecl> Parser::parseOperatorOverload(FieldDecl::Visibility vis) {
    consume(TokenType::KW_OP, "Expected 'op'");

    Token opToken = advance();
    std::string opName = "op$";

    switch (opToken.type) {
        case TokenType::PLUS: opName += "add"; break;
        case TokenType::MINUS: opName += "sub"; break;
        case TokenType::STAR: opName += "mul"; break;
        case TokenType::SLASH: opName += "div"; break;
        case TokenType::PERCENT: opName += "mod"; break;
        case TokenType::EQ: opName += "eq"; break;
        case TokenType::NEQ: opName += "neq"; break;
        case TokenType::LT: opName += "lt"; break;
        case TokenType::LTE: opName += "lte"; break;
        case TokenType::GT: opName += "gt"; break;
        case TokenType::GTE: opName += "gte"; break;
        case TokenType::NOT: opName += "not"; break;
        case TokenType::LBRACKET:
            consume(TokenType::RBRACKET, "Expected ']' after '['");
            opName += "index_get";
            break;
        default:
            throw error("Invalid operator for overloading");
    }

    consume(TokenType::LPAREN, "Expected '(' after operator");
    std::vector<Parameter> params = parseParameters();
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    std::string returnType;
    if (check(TokenType::ARROW)) {
        advance();
        returnType = parseType();
    } else if (check(TokenType::MINUS)) {
        advance();
        if (check(TokenType::GT)) {
            advance();
            returnType = parseType();
        } else {
            throw error("Expected '>' after '-' in return type");
        }
    }

    StmtPtr body = parseBlock();

    auto method = std::make_unique<FunctionDecl>(opName, params, returnType,
                                                 std::move(body));
    method->isOperator = true;
    switch (vis) {
        case FieldDecl::Visibility::PUBLIC:
            method->visibility = FunctionDecl::Visibility::PUBLIC;
            break;
        case FieldDecl::Visibility::PRIVATE:
            method->visibility = FunctionDecl::Visibility::PRIVATE;
            break;
        case FieldDecl::Visibility::PROTECTED:
            method->visibility = FunctionDecl::Visibility::PROTECTED;
            break;
    }
    return method;
}

StmtPtr Parser::parseStatement() {
    if (match({TokenType::KW_LET})) return parseVarDecl();
    if (match({TokenType::KW_IF})) return parseIfStmt();
    if (match({TokenType::KW_WHILE})) return parseWhileStmt();
    if (match({TokenType::KW_FOR})) return parseForStmt();
    if (match({TokenType::KW_LOOP})) return parseLoopStmt();
    if (match({TokenType::KW_RETURN})) return parseReturnStmt();
    if (match({TokenType::KW_BREAK})) return parseBreakStmt();
    if (match({TokenType::KW_CONTINUE})) return parseContinueStmt();
    if (check(TokenType::LBRACE)) return parseBlock();

    return parseExprStmt();
}

StmtPtr Parser::parseVarDecl() {
    Token letToken = previous(); // We just matched KW_LET
    Token varName = consume(TokenType::IDENTIFIER, "Expected variable name");

    std::string type;
    if (match({TokenType::COLON})) {
        type = parseType();
    }

    ExprPtr initializer = nullptr;
    if (match({TokenType::ASSIGN})) {
        initializer = parseExpression();
    }

    auto stmt = std::make_unique<VarDeclStmt>(varName.lexeme, type, std::move(initializer));
    stmt->line = letToken.line;
    stmt->column = letToken.column;

    std::cerr << "[PARSER] VarDecl at line " << stmt->line << "\n";

    return stmt;
}

StmtPtr Parser::parseIfStmt() {
    Token ifToken = previous(); // We just matched KW_IF
    ExprPtr condition = parseExpression();
    StmtPtr thenBranch = parseBlock();

    StmtPtr elseBranch = nullptr;
    if (match({TokenType::KW_ELSE})) {
        if (check(TokenType::KW_IF)) {
            advance();
            elseBranch = parseIfStmt();
        } else {
            elseBranch = parseBlock();
        }
    }

    auto stmt = std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch),
                                         std::move(elseBranch));
    stmt->line = ifToken.line;
    stmt->column = ifToken.column;

    std::cerr << "[PARSER] IfStmt at line " << stmt->line << "\n";

    return stmt;
}

StmtPtr Parser::parseWhileStmt() {
    Token whileToken = previous(); // We just matched KW_WHILE
    ExprPtr condition = parseExpression();
    StmtPtr body = parseBlock();

    auto stmt = std::make_unique<WhileStmt>(std::move(condition), std::move(body));
    stmt->line = whileToken.line;
    stmt->column = whileToken.column;
    return stmt;
}

StmtPtr Parser::parseForStmt() {
    Token forToken = previous(); // We just matched KW_FOR
    Token varName = consume(TokenType::IDENTIFIER, "Expected variable name");
    consume(TokenType::KW_IN, "Expected 'in' after variable");
    ExprPtr iterable = parseExpression();
    StmtPtr body = parseBlock();

    auto stmt = std::make_unique<ForStmt>(varName.lexeme, std::move(iterable),
                                          std::move(body));
    stmt->line = forToken.line;
    stmt->column = forToken.column;
    return stmt;
}

StmtPtr Parser::parseLoopStmt() {
    Token loopToken = previous(); // We just matched KW_LOOP
    StmtPtr body = parseBlock();

    auto stmt = std::make_unique<LoopStmt>(std::move(body));
    stmt->line = loopToken.line;
    stmt->column = loopToken.column;
    return stmt;
}

StmtPtr Parser::parseReturnStmt() {
    Token returnToken = previous(); // We just matched KW_RETURN
    ExprPtr value = nullptr;
    if (!check(TokenType::SEMICOLON) && !check(TokenType::RBRACE) && !check(TokenType::EOF_TOKEN)) {
        value = parseExpression();
    }

    auto stmt = std::make_unique<ReturnStmt>(std::move(value));
    stmt->line = returnToken.line;
    stmt->column = returnToken.column;
    return stmt;
}

StmtPtr Parser::parseBreakStmt() {
    Token breakToken = previous(); // We just matched KW_BREAK
    auto stmt = std::make_unique<BreakStmt>();
    stmt->line = breakToken.line;
    stmt->column = breakToken.column;
    return stmt;
}

StmtPtr Parser::parseContinueStmt() {
    Token continueToken = previous(); // We just matched KW_CONTINUE
    auto stmt = std::make_unique<ContinueStmt>();
    stmt->line = continueToken.line;
    stmt->column = continueToken.column;
    return stmt;
}

StmtPtr Parser::parseBlock() {
    Token lbraceToken = peek(); // Capture before consuming
    consume(TokenType::LBRACE, "Expected '{'");

    std::vector<StmtPtr> statements;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        statements.push_back(parseStatement());
    }

    consume(TokenType::RBRACE, "Expected '}'");

    auto stmt = std::make_unique<BlockStmt>(std::move(statements));
    stmt->line = lbraceToken.line;
    stmt->column = lbraceToken.column;
    return stmt;
}

StmtPtr Parser::parseExprStmt() {
    Token firstToken = peek(); // Get current token for line info
    ExprPtr expr = parseExpression();

    auto stmt = std::make_unique<ExprStmt>(std::move(expr));
    stmt->line = firstToken.line;
    stmt->column = firstToken.column;

    std::cerr << "[PARSER] ExprStmt at line " << stmt->line << "\n";

    return stmt;
}

ExprPtr Parser::parseExpression() {
    return parseAssignment();
}

ExprPtr Parser::parseAssignment() {
    Token tok = peek(); // Capture before parsing
    ExprPtr expr = parseLogicalOr();

    if (match({TokenType::ASSIGN})) {
        ExprPtr value = parseAssignment();
        auto assignExpr = std::make_unique<AssignExpr>(std::move(expr), std::move(value));
        assignExpr->line = tok.line;
        assignExpr->column = tok.column;
        return assignExpr;
    }

    if (match({TokenType::PLUS_EQ})) {
        ExprPtr value = parseAssignment();
        auto compExpr = std::make_unique<CompoundAssignExpr>(CompoundAssignExpr::Op::ADD,
                                                              std::move(expr), std::move(value));
        compExpr->line = tok.line;
        compExpr->column = tok.column;
        return compExpr;
    }

    if (match({TokenType::MINUS_EQ})) {
        ExprPtr value = parseAssignment();
        auto compExpr = std::make_unique<CompoundAssignExpr>(CompoundAssignExpr::Op::SUB,
                                                              std::move(expr), std::move(value));
        compExpr->line = tok.line;
        compExpr->column = tok.column;
        return compExpr;
    }

    return expr;
}

ExprPtr Parser::parseLogicalOr() {
    Token tok = peek();
    ExprPtr expr = parseLogicalAnd();

    while (match({TokenType::OR})) {
        ExprPtr right = parseLogicalAnd();
        auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::OR, std::move(expr),
                                                     std::move(right));
        binExpr->line = tok.line;
        binExpr->column = tok.column;
        expr = std::move(binExpr);
    }

    return expr;
}

ExprPtr Parser::parseLogicalAnd() {
    Token tok = peek();
    ExprPtr expr = parseEquality();

    while (match({TokenType::AND})) {
        ExprPtr right = parseEquality();
        auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::AND, std::move(expr),
                                                     std::move(right));
        binExpr->line = tok.line;
        binExpr->column = tok.column;
        expr = std::move(binExpr);
    }

    return expr;
}

ExprPtr Parser::parseEquality() {
    Token tok = peek();
    ExprPtr expr = parseComparison();

    while (true) {
        if (match({TokenType::EQ})) {
            ExprPtr right = parseComparison();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::EQ, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else if (match({TokenType::NEQ})) {
            ExprPtr right = parseComparison();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::NEQ, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parseComparison() {
    Token tok = peek();
    ExprPtr expr = parseTerm();

    while (true) {
        if (match({TokenType::LT})) {
            ExprPtr right = parseTerm();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::LT, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else if (match({TokenType::LTE})) {
            ExprPtr right = parseTerm();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::LTE, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else if (match({TokenType::GT})) {
            ExprPtr right = parseTerm();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::GT, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else if (match({TokenType::GTE})) {
            ExprPtr right = parseTerm();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::GTE, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parseTerm() {
    Token tok = peek();
    ExprPtr expr = parseFactor();

    while (true) {
        if (match({TokenType::PLUS})) {
            ExprPtr right = parseFactor();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::ADD, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else if (match({TokenType::MINUS})) {
            ExprPtr right = parseFactor();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::SUB, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parseFactor() {
    Token tok = peek();
    ExprPtr expr = parseUnary();

    while (true) {
        if (match({TokenType::STAR})) {
            ExprPtr right = parseUnary();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::MUL, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else if (match({TokenType::SLASH})) {
            ExprPtr right = parseUnary();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::DIV, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else if (match({TokenType::PERCENT})) {
            ExprPtr right = parseUnary();
            auto binExpr = std::make_unique<BinaryExpr>(BinaryExpr::Op::MOD, std::move(expr),
                                                         std::move(right));
            binExpr->line = tok.line;
            binExpr->column = tok.column;
            expr = std::move(binExpr);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parseUnary() {
    Token tok = peek();

    if (match({TokenType::NOT})) {
        ExprPtr operand = parseUnary();
        auto expr = std::make_unique<UnaryExpr>(UnaryExpr::Op::NOT, std::move(operand));
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    if (match({TokenType::MINUS})) {
        ExprPtr operand = parseUnary();
        auto expr = std::make_unique<UnaryExpr>(UnaryExpr::Op::NEG, std::move(operand));
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();

    while (true) {
        if (match({TokenType::LPAREN})) {
            expr = parseCall(std::move(expr));
        } else if (match({TokenType::DOT})) {
            expr = parseFieldAccess(std::move(expr));
        } else if (match({TokenType::LBRACKET})) {
            expr = parseIndex(std::move(expr));
        } else if (match({TokenType::KW_AS})) {
            Token asTok = previous();
            std::string targetType = parseType();
            auto castExpr = std::make_unique<CastExpr>(std::move(expr), targetType);
            castExpr->line = asTok.line;
            castExpr->column = asTok.column;
            expr = std::move(castExpr);
        } else if (match({TokenType::KW_IS})) {
            Token isTok = previous();
            std::string targetType = parseType();
            auto isExpr = std::make_unique<IsExpr>(std::move(expr), targetType);
            isExpr->line = isTok.line;
            isExpr->column = isTok.column;
            expr = std::move(isExpr);
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    Token tok = peek(); // Capture token BEFORE advancing

    if (match({TokenType::INT})) {
        int64_t value = std::stoll(previous().lexeme);
        auto expr = std::make_unique<LiteralExpr>(value);
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    if (match({TokenType::FLOAT})) {
        double value = std::stod(previous().lexeme);
        auto expr = std::make_unique<LiteralExpr>(value);
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    if (match({TokenType::BOOL})) {
        bool value = previous().lexeme == "true";
        auto expr = std::make_unique<LiteralExpr>(value);
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    if (match({TokenType::STRING})) {
        auto expr = std::make_unique<LiteralExpr>(previous().lexeme);
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    if (match({TokenType::NULLVAL})) {
        auto expr = std::make_unique<LiteralExpr>(LiteralExpr::Null());
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    if (match({TokenType::IDENTIFIER})) {
        auto expr = std::make_unique<IdentifierExpr>(previous().lexeme);
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    if (match({TokenType::KW_NEW})) {
        return parseNewExpr();
    }

    if (match({TokenType::LBRACKET})) {
        return parseListLiteral();
    }

    if (match({TokenType::LPAREN})) {
        ExprPtr expr = parseExpression();
        consume(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    if (match({TokenType::KW_SELF})) {
        auto expr = std::make_unique<IdentifierExpr>("self");
        expr->line = tok.line;
        expr->column = tok.column;
        return expr;
    }

    throw error("Expected expression");
}

ExprPtr Parser::parseCall(ExprPtr callee) {
    Token tok = previous(); // LPAREN was just matched
    std::vector<ExprPtr> arguments = parseArguments();
    consume(TokenType::RPAREN, "Expected ')' after arguments");

    auto expr = std::make_unique<CallExpr>(std::move(callee), std::move(arguments));
    expr->line = tok.line;
    expr->column = tok.column;
    return expr;
}

ExprPtr Parser::parseFieldAccess(ExprPtr object) {
    Token dotToken = previous(); // DOT was just matched
    Token field = consume(TokenType::IDENTIFIER, "Expected field name after '.'");

    auto expr = std::make_unique<FieldAccessExpr>(std::move(object), field.lexeme);
    expr->line = dotToken.line;
    expr->column = dotToken.column;
    return expr;
}

ExprPtr Parser::parseIndex(ExprPtr object) {
    Token lbracketToken = previous(); // LBRACKET was just matched
    ExprPtr index = parseExpression();
    consume(TokenType::RBRACKET, "Expected ']' after index");

    auto expr = std::make_unique<IndexExpr>(std::move(object), std::move(index));
    expr->line = lbracketToken.line;
    expr->column = lbracketToken.column;
    return expr;
}

ExprPtr Parser::parseNewExpr() {
    Token newToken = previous(); // KW_NEW was just matched
    Token className = consume(TokenType::IDENTIFIER, "Expected class name after 'new'");

    std::vector<std::string> typeParams = parseTypeParams();

    consume(TokenType::LPAREN, "Expected '(' after class name");
    std::vector<ExprPtr> arguments = parseArguments();
    consume(TokenType::RPAREN, "Expected ')' after arguments");

    auto expr = std::make_unique<NewExpr>(className.lexeme, typeParams, std::move(arguments));
    expr->line = newToken.line;
    expr->column = newToken.column;
    return expr;
}

ExprPtr Parser::parseListLiteral() {
    Token lbracketToken = previous(); // LBRACKET was just matched
    std::vector<ExprPtr> elements;

    if (!check(TokenType::RBRACKET)) {
        do {
            elements.push_back(parseExpression());
        } while (match({TokenType::COMMA}));
    }

    consume(TokenType::RBRACKET, "Expected ']' after list elements");

    auto expr = std::make_unique<ListExpr>(std::move(elements));
    expr->line = lbracketToken.line;
    expr->column = lbracketToken.column;
    return expr;
}

ExprPtr Parser::parseDictLiteral() const {
    throw error("Dictionary literals not yet implemented");
}

std::string Parser::parseType() {
    const Token typeToken = consume(TokenType::IDENTIFIER, "Expected type name");
    std::string type = typeToken.lexeme;

    if (match({TokenType::LBRACKET})) {
        type += "[";
        type += parseType();

        while (match({TokenType::COMMA})) {
            type += ",";
            type += parseType();
        }

        consume(TokenType::RBRACKET, "Expected ']' after type parameters");
        type += "]";
    }

    return type;
}

std::vector<std::string> Parser::parseTypeParams() {
    std::vector<std::string> typeParams;

    if (match({TokenType::LBRACKET})) {
        do {
            Token typeParam = consume(TokenType::IDENTIFIER, "Expected type parameter");
            typeParams.push_back(typeParam.lexeme);
        } while (match({TokenType::COMMA}));

        consume(TokenType::RBRACKET, "Expected ']' after type parameters");
    }

    return typeParams;
}

std::vector<Parameter> Parser::parseParameters() {
    std::vector<Parameter> params;

    if (!check(TokenType::RPAREN)) {
        do {
            Token paramName = consume(TokenType::IDENTIFIER, "Expected parameter name");
            consume(TokenType::COLON, "Expected ':' after parameter name");
            std::string paramType = parseType();
            params.emplace_back(paramName.lexeme, paramType);
        } while (match({TokenType::COMMA}));
    }

    return params;
}

std::vector<ExprPtr> Parser::parseArguments() {
    std::vector<ExprPtr> arguments;

    if (!check(TokenType::RPAREN)) {
        do {
            arguments.push_back(parseExpression());
        } while (match({TokenType::COMMA}));
    }

    return arguments;
}

FieldDecl::Visibility Parser::parseVisibility() {
    if (match({TokenType::KW_PUB})) {
        return FieldDecl::Visibility::PUBLIC;
    } else if (match({TokenType::KW_PRIV})) {
        return FieldDecl::Visibility::PRIVATE;
    } else if (match({TokenType::KW_PROT})) {
        return FieldDecl::Visibility::PROTECTED;
    }
    return FieldDecl::Visibility::PUBLIC;
}

std::string Parser::parseQualifiedName() {
    const Token first = consume(TokenType::IDENTIFIER, "Expected identifier");
    std::string name = first.lexeme;

    while (match({TokenType::DOT})) {
        Token next = consume(TokenType::IDENTIFIER, "Expected identifier after '.'");
        name += "." + next.lexeme;
    }

    return name;
}