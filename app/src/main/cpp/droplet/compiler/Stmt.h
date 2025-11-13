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
 *  File: Stmt
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_STMT_H
#define DROPLET_STMT_H

#include <memory>
#include <optional>
#include <vector>

#include "Expr.h"

struct Stmt {
    uint32_t line = 0;
    uint32_t column = 0;

    virtual ~Stmt() = default;
};

struct ExprStmt final : Stmt {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e) : expr(std::move(e)) {}
};

struct VarDeclStmt final : Stmt {
    std::string name;
    std::string type;  // Optional type annotation
    ExprPtr initializer;

    VarDeclStmt(std::string n, std::string t, ExprPtr init): name(std::move(n)), type(std::move(t)), initializer(std::move(init)) {}
};

struct BlockStmt final : Stmt {
    std::vector<StmtPtr> statements;
    explicit BlockStmt(std::vector<StmtPtr> stmts) : statements(std::move(stmts)) {}
};

struct IfStmt final : Stmt {
    ExprPtr condition;
    StmtPtr thenBranch;
    StmtPtr elseBranch;  // nullptr if no else

    IfStmt(ExprPtr cond, StmtPtr then, StmtPtr els = nullptr)
        : condition(std::move(cond)), thenBranch(std::move(then)),
          elseBranch(std::move(els)) {}
};

struct WhileStmt final : Stmt {
    ExprPtr condition;
    StmtPtr body;

    WhileStmt(ExprPtr cond, StmtPtr b)
        : condition(std::move(cond)), body(std::move(b)) {}
};

struct ForStmt final : Stmt {
    std::string variable;
    ExprPtr iterable;  // Range or collection
    StmtPtr body;

    ForStmt(std::string var, ExprPtr iter, StmtPtr b)
        : variable(std::move(var)), iterable(std::move(iter)), body(std::move(b)) {}
};

struct LoopStmt final : Stmt {
    StmtPtr body;
    explicit LoopStmt(StmtPtr b) : body(std::move(b)) {}
};

struct ReturnStmt final : Stmt {
    ExprPtr value;  // nullptr for empty return
    explicit ReturnStmt(ExprPtr v = nullptr) : value(std::move(v)) {}
};

struct BreakStmt final : Stmt {};

struct ContinueStmt final : Stmt {};

struct Parameter {
    std::string name;
    std::string type;

    Parameter(std::string n, std::string t) : name(std::move(n)), type(std::move(t)) {}
};

struct FFIInfo {
    std::string libPath;
    std::string sig; // Signature string
};

struct FunctionDecl final : Stmt {
    enum class Visibility { PUBLIC, PRIVATE, PROTECTED };

    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
    StmtPtr body;
    bool isStatic;
    bool isSealed;
    Visibility visibility;
    bool isOperator;  // true if this is an operator overload
    bool mayReturnError;  // for type T! : meaning either T or er
    std::optional<FFIInfo> ffi;

    FunctionDecl(std::string n, std::vector<Parameter> p, std::string ret,
                 StmtPtr b, const bool stat = false, const bool seal = false,
                 const Visibility vis = Visibility::PUBLIC, const bool isOp = false, const bool mayReturnError = false)
        : name(std::move(n)), params(std::move(p)), returnType(std::move(ret)),
          body(std::move(b)), isStatic(stat), isSealed(seal),
          visibility(vis), isOperator(isOp), mayReturnError(mayReturnError) {}
};

struct FieldDecl {
    enum class Visibility { PUBLIC, PRIVATE, PROTECTED };

    std::string name;
    std::string type;
    ExprPtr initializer;  // nullptr if no initializer
    bool isStatic;
    Visibility visibility;

    FieldDecl(std::string n, std::string t, ExprPtr init = nullptr,
              const bool stat = false, const Visibility vis = Visibility::PUBLIC)
        : name(std::move(n)), type(std::move(t)), initializer(std::move(init)),
          isStatic(stat), visibility(vis) {}
};

struct ClassDecl final : Stmt {
    std::string name;
    std::vector<std::string> typeParams;  // For generics
    std::string parentClass;  // Empty if no parent
    std::vector<FieldDecl> fields;
    std::vector<std::unique_ptr<FunctionDecl>> methods;
    std::unique_ptr<FunctionDecl> constructor;  // The 'new' function
    bool isSealed;

    explicit ClassDecl(std::string n, std::vector<std::string> types = {}, std::string parent = "", const bool seal = false)
        : name(std::move(n)), typeParams(std::move(types)),
          parentClass(std::move(parent)), isSealed(seal) {}
};

struct ImportStmt final : Stmt {
    std::string modulePath;  // e.g., "std.collections"
    std::vector<std::string> symbols;  // Empty for wildcard import
    bool isWildcard;

    ImportStmt(std::string path, std::vector<std::string> syms, const bool wild = false)
        : modulePath(std::move(path)), symbols(std::move(syms)),
          isWildcard(wild) {}
};

struct ModuleDecl final : Stmt {
    std::string moduleName;  // e.g., "com.example.math"
    explicit ModuleDecl(std::string name) : moduleName(std::move(name)) {}
};

struct FFIDecl final : Stmt {
    std::string libName;  // e.g., "libmath.so"
    std::string symbolName;  // C function name
    std::string signature;  // e.g., "ff->f"
    std::string dropletName;  // Droplet function name
    std::vector<Parameter> params;
    std::string returnType;

    FFIDecl(std::string lib, std::string sym, std::string sig,
            std::string name, std::vector<Parameter> p, std::string ret)
        : libName(std::move(lib)), symbolName(std::move(sym)),
          signature(std::move(sig)), dropletName(std::move(name)),
          params(std::move(p)), returnType(std::move(ret)) {}
};


#endif //DROPLET_STMT_H