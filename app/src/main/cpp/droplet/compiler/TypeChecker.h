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
 *  File: TypeChecker
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_TYPECHECKER_H
#define DROPLET_TYPECHECKER_H


#include "Stmt.h"
#include "Expr.h"
#include "Program.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <stdexcept>

#include "ModuleLoader.h"


struct Type {
    enum class Kind {
        INT, FLOAT, BOOL, STRING, NULL_TYPE, VOID,
        LIST, DICT, OBJECT, FUNCTION, GENERIC, UNKNOWN
    };

    Kind kind;
    std::string className;  // For OBJECT types
    std::vector<std::shared_ptr<Type>> typeParams;  // For LIST, DICT, generics
    std::vector<std::shared_ptr<Type>> paramTypes;  // For FUNCTION types
    std::shared_ptr<Type> returnType;  // For FUNCTION types
    bool canReturnError = false;  // For FUNCTION types
    bool isChecked = false;      // tracks if error has been handled
    FieldDecl::Visibility visibility;

    Type(Kind k) : kind(k) {}
    Type(Kind k, std::string cls) : kind(k), className(std::move(cls)) {}

    static std::shared_ptr<Type> Int() { return std::make_shared<Type>(Kind::INT); }
    static std::shared_ptr<Type> Float() { return std::make_shared<Type>(Kind::FLOAT); }
    static std::shared_ptr<Type> Bool() { return std::make_shared<Type>(Kind::BOOL); }
    static std::shared_ptr<Type> String() { return std::make_shared<Type>(Kind::STRING); }
    static std::shared_ptr<Type> Null() { return std::make_shared<Type>(Kind::NULL_TYPE); }
    static std::shared_ptr<Type> Void() { return std::make_shared<Type>(Kind::VOID); }
    static std::shared_ptr<Type> Unknown() { return std::make_shared<Type>(Kind::UNKNOWN); }

    static std::shared_ptr<Type> List(std::shared_ptr<Type> elemType) {
        auto t = std::make_shared<Type>(Kind::LIST);
        t->typeParams.push_back(elemType);
        return t;
    }

    static std::shared_ptr<Type> Dict(std::shared_ptr<Type> keyType, std::shared_ptr<Type> valType) {
        auto t = std::make_shared<Type>(Kind::DICT);
        t->typeParams.push_back(keyType);
        t->typeParams.push_back(valType);
        return t;
    }

    static std::shared_ptr<Type> Object(const std::string& className) {
        return std::make_shared<Type>(Kind::OBJECT, className);
    }

    std::string toString() const {
        std::string append = "";
        if (canReturnError && !isChecked) {
            append = "!";
        }
        switch (kind) {
            case Kind::INT: return "int" + append;
            case Kind::FLOAT: return "float"  + append;
            case Kind::BOOL: return "bool" + append;
            case Kind::STRING: return "str" + append;
            case Kind::NULL_TYPE: return "null" + append;
            case Kind::VOID: return "void" + append;
            case Kind::LIST:
                return "list[" + (typeParams.empty() ? "?" : typeParams[0]->toString()) + "]" + append;
            case Kind::DICT:
                return "dict[" +
                       (typeParams.size() < 2 ? "?,?" : typeParams[0]->toString() + "," + typeParams[1]->toString()) + "]" + append;
            case Kind::OBJECT:
                return className + append;
            case Kind::FUNCTION:
                return "fn(...)" + append;
            case Kind::GENERIC:
                return className + append;  // Generic type parameter name
            case Kind::UNKNOWN:
                return "?" + append;
        }

        return "?" + append;
    }

    bool isNumeric() const {
        return kind == Kind::INT || kind == Kind::FLOAT;
    }

    bool equals(const std::shared_ptr<Type>& other) const {
        if (kind != other->kind) return false;

        switch (kind) {
            case Kind::OBJECT:
            case Kind::GENERIC:
                return className == other->className;
            case Kind::LIST:
                return !typeParams.empty() && !other->typeParams.empty() &&
                       typeParams[0]->equals(other->typeParams[0]);
            case Kind::DICT:
                return typeParams.size() >= 2 && other->typeParams.size() >= 2 &&
                       typeParams[0]->equals(other->typeParams[0]) &&
                       typeParams[1]->equals(other->typeParams[1]);
            default:
                return true;
        }
    }
};

struct Symbol {
    enum class Kind { VARIABLE, FUNCTION, CLASS, FIELD, METHOD, PARAMETER };

    Kind kind;
    std::string name;
    std::shared_ptr<Type> type;
    bool isStatic = false;
    bool isSealed = false;
    FunctionDecl::Visibility visibility = FunctionDecl::Visibility::PUBLIC;

    Symbol(Kind k, std::string n, std::shared_ptr<Type> t)
        : kind(k), name(std::move(n)), type(std::move(t)) {}
};

class Scope {
public:
    std::unordered_map<std::string, Symbol> symbols;
    Scope* parent = nullptr;

    explicit Scope(Scope* p = nullptr) : parent(p) {}

    void define(const Symbol& symbol) {
        symbols.insert_or_assign(symbol.name, symbol);
    }

    Symbol* resolve(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) return &it->second;
        if (parent) return parent->resolve(name);
        return nullptr;
    }

    bool hasLocal(const std::string& name) const {
        return symbols.find(name) != symbols.end();
    }
};

struct ClassInfo {
    std::string name;
    std::string parentClass;
    std::vector<std::string> typeParams;
    std::unordered_map<std::string, std::shared_ptr<Type>> fields;
    std::unordered_map<std::string, FunctionDecl*> methods;
    FunctionDecl* constructor = nullptr;
    bool isSealed = false;

    // Field offsets for inheritance
    std::unordered_map<std::string, int> fieldOffsets;
    int totalFieldCount = 0;
};

class TypeChecker {
public:
    TypeChecker() {
        globalScope = std::make_shared<Scope>();
        currentScope = globalScope.get();
        registerBuiltinTypes();
    }

    struct FFIInfo {
        std::string libPath;
        std::string sig;
        std::shared_ptr<Type> returnType;
        std::vector<std::shared_ptr<Type>> paramTypes;
    };

    std::unordered_map<std::string, FFIInfo> ffiFunctions;


    void setModuleLoader(ModuleLoader* loader) {
        moduleLoader = loader;
    }

    // Main entry point
    void check(const Program &program);

    // Error reporting
    struct TypeError : public std::runtime_error {
        TypeError(const std::string& msg) : std::runtime_error("Type Error: " + msg) {}
    };

    const std::unordered_map<std::string, ClassInfo>& getClassInfo() const {
        return classes;
    }
    void registerFFIFunctions(const std::vector<std::unique_ptr<FunctionDecl>> &funcs);

private:
    std::shared_ptr<Scope> globalScope;
    Scope* currentScope;
    std::unordered_map<std::string, ClassInfo> classes;
    std::string currentClassName;
    std::shared_ptr<Type> currentFunctionReturnType;
    bool currentFunctionMayReturnError;
    ModuleLoader* moduleLoader = nullptr;
    bool isInIsErrorCheck = false;

    // checking error
    void enforceErrorCheck(const std::string& varName, const std::shared_ptr<Type>& type);
    bool blockDefinitelyReturns(const Stmt* stmt);

    // process imports
    void processImports(const Program& program);
    void importSymbolsFromModule(const ModuleInfo *module, const ImportStmt *import);

    // Built-in types registration
    void registerBuiltinTypes();
    void registerBuiltins() const;

    // Class analysis
    void analyzeClass(const ClassDecl* classDecl);
    void analyzeClassHierarchy();
    void computeFieldOffsets(ClassInfo& info);

    // Type resolution
    std::shared_ptr<Type> resolveType(const std::string& typeStr);
    std::shared_ptr<Type> resolveTypeWithGenerics(const std::string& typeStr,
                                                    const std::vector<std::string>& typeParams);

    // Declaration checking
    void checkFunction(const FunctionDecl* func);
    void checkFieldDecl(const FieldDecl &field);

    // Statement checking
    void checkStmt(Stmt* stmt);
    void checkVarDecl(const VarDeclStmt* stmt);
    void checkBlock(const BlockStmt* stmt);
    void checkIf(const IfStmt* stmt);
    void checkWhile(const WhileStmt* stmt);
    void checkFor(const ForStmt* stmt);
    void checkLoop(const LoopStmt* stmt);
    void checkReturn(const ReturnStmt* stmt);
    void checkExprStmt(const ExprStmt* stmt);

    // Expression checking
    std::shared_ptr<Type> checkExpr(Expr* expr);

    static std::shared_ptr<Type> checkLiteral(const LiteralExpr* expr);
    std::shared_ptr<Type> checkIdentifier(const IdentifierExpr* expr);
    std::shared_ptr<Type> checkBinary(BinaryExpr* expr);
    std::shared_ptr<Type> checkUnary(const UnaryExpr* expr);
    std::shared_ptr<Type> checkAssign(const AssignExpr* expr);
    std::shared_ptr<Type> checkCompoundAssign(const CompoundAssignExpr* expr);
    std::shared_ptr<Type> checkCall(const CallExpr* expr);

    static bool isDescendant(const std::string &childName, const std::string &potentialAncestor, const std::unordered_map<std::string, ClassInfo> &classes);

    std::shared_ptr<Type> checkFieldAccess(const FieldAccessExpr* expr);
    std::shared_ptr<Type> checkIndex(const IndexExpr* expr);
    std::shared_ptr<Type> checkNew(const NewExpr* expr);
    std::shared_ptr<Type> checkList(const ListExpr* expr);
    std::shared_ptr<Type> checkDict(const DictExpr* expr);
    std::shared_ptr<Type> checkCast(const CastExpr* expr);
    std::shared_ptr<Type> checkIs(const IsExpr* expr);

    // Type compatibility
    bool isAssignable(const std::shared_ptr<Type>& target, const std::shared_ptr<Type>& source);
    bool isSubclass(const std::string& child, const std::string& parent);

    static std::shared_ptr<Type> promoteNumeric(const std::shared_ptr<Type>& a, const std::shared_ptr<Type>& b);

    // Scope management
    void enterScope() {
        auto newScope = std::make_shared<Scope>(currentScope);
        currentScope = newScope.get();
        scopes.push_back(newScope);
    }

    void exitScope() {
        if (currentScope->parent) {
            currentScope = currentScope->parent;
        }
    }

    std::vector<std::shared_ptr<Scope>> scopes;  // Keep scopes alive

    // Utility
    void error(const std::string& message) {
        throw TypeError(message);
    }
};


#endif //DROPLET_TYPECHECKER_H