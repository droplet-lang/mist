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
 *  File: AST
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_EXPR_H
#define DROPLET_EXPR_H
#include <memory>
#include <variant>
#include <vector>

struct Type;
struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

struct Expr {
    virtual ~Expr() = default;
    std::shared_ptr<Type> type;

    uint32_t line = 0;
    uint32_t column = 0;
};

struct LiteralExpr final : Expr {
    enum class Type { INT, FLOAT, BOOL, STRING, NULLVAL };

    Type type;
    std::variant<int64_t, double, bool, std::string> value;

    explicit LiteralExpr(int64_t v) : type(Type::INT), value(v) {}
    explicit LiteralExpr(double v) : type(Type::FLOAT), value(v) {}
    explicit LiteralExpr(bool v) : type(Type::BOOL), value(v) {}
    explicit LiteralExpr(std::string v, const bool isStr = true) : type(isStr ? Type::STRING : Type::NULLVAL), value(std::move(v)) {}
    static LiteralExpr Null() { return LiteralExpr("null", false); }
};

struct IdentifierExpr final : Expr {
    std::string name;
    explicit IdentifierExpr(std::string n) : name(std::move(n)) {}
};

struct BinaryExpr final : Expr {
    enum class Op {
        ADD, SUB, MUL, DIV, MOD,
        EQ, NEQ, LT, LTE, GT, GTE,
        AND, OR
    };
    Op op;
    ExprPtr left;
    ExprPtr right;
    std::string operatorMethodName;
    bool hasOperatorOverload = false;

    BinaryExpr(Op o, ExprPtr l, ExprPtr r)
        : op(o), left(std::move(l)), right(std::move(r)) {}
};

struct UnaryExpr final : Expr {
    enum class Op { NEG, NOT };
    Op op;
    ExprPtr operand;

    UnaryExpr(const Op o, ExprPtr e) : op(o), operand(std::move(e)) {}
};

struct AssignExpr final : Expr {
    ExprPtr target;  // Can be IdentifierExpr, FieldAccessExpr, or IndexExpr
    ExprPtr value;

    AssignExpr(ExprPtr t, ExprPtr v): target(std::move(t)), value(std::move(v)) {}
};

struct CompoundAssignExpr final : Expr {
    enum class Op { ADD, SUB };
    Op op;
    ExprPtr target;
    ExprPtr value;

    CompoundAssignExpr(const Op o, ExprPtr t, ExprPtr v): op(o), target(std::move(t)), value(std::move(v)) {}
};

struct CallExpr final : Expr {
    ExprPtr callee;  // Can be IdentifierExpr or FieldAccessExpr
    std::vector<ExprPtr> arguments;

    CallExpr(ExprPtr c, std::vector<ExprPtr> args): callee(std::move(c)), arguments(std::move(args)) {}
};

struct FieldAccessExpr final : Expr {
    ExprPtr object;
    std::string field;

    FieldAccessExpr(ExprPtr obj, std::string f): object(std::move(obj)), field(std::move(f)) {}
};

struct IndexExpr final : Expr {
    ExprPtr object;
    ExprPtr index;

    IndexExpr(ExprPtr obj, ExprPtr idx): object(std::move(obj)), index(std::move(idx)) {}
};

struct NewExpr final : Expr {
    std::string className;
    std::vector<std::string> typeParams;  // For generics: Box[int]
    std::vector<ExprPtr> arguments;

    NewExpr(std::string cls, std::vector<std::string> types, std::vector<ExprPtr> args): className(std::move(cls)), typeParams(std::move(types)), arguments(std::move(args)) {}
};

struct ListExpr final : Expr {
    std::vector<ExprPtr> elements;

    explicit ListExpr(std::vector<ExprPtr> elems) : elements(std::move(elems)) {}
};

struct DictExpr final : Expr {
    std::vector<std::pair<ExprPtr, ExprPtr>> pairs;

    explicit DictExpr(std::vector<std::pair<ExprPtr, ExprPtr>> p): pairs(std::move(p)) {}
};

struct CastExpr final : Expr {
    ExprPtr expr;
    std::string targetType;

    CastExpr(ExprPtr e, std::string type): expr(std::move(e)), targetType(std::move(type)) {}
};

struct IsExpr final : Expr {
    ExprPtr expr;
    std::string targetType;

    IsExpr(ExprPtr e, std::string type): expr(std::move(e)), targetType(std::move(type)) {}
};

#endif