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

#include "TypeChecker.h"
#include <algorithm>
#include <iostream>
#include <unordered_set>

void TypeChecker::registerFFIFunctions(const std::vector<std::unique_ptr<FunctionDecl> > &funcs) {
    for (auto &f: funcs) {
        if (f->ffi.has_value()) {
            ::FFIInfo ffi = f->ffi.value();

            // Map sig to actual parameter & return types
            std::vector<std::shared_ptr<Type> > paramTypes;
            for (auto &p: f->params) {
                paramTypes.push_back(resolveType(p.type));
            }

            std::shared_ptr<Type> returnType = f->returnType.empty() ? Type::Void() : resolveType(f->returnType);

            ffiFunctions[f->name] = {
                ffi.libPath,
                ffi.sig,
                returnType,
                paramTypes
            };
        }
    }
}

void TypeChecker::processImports(const Program &program) {
    for (auto &import: program.imports) {
        std::cerr << "Processing import: " << import->modulePath << "\n";

        // Load the module (this just loads AST, doesn't type check)
        ModuleInfo *module = moduleLoader->loadModule(import->modulePath);
        if (!module) {
            error("Failed to load module: " + import->modulePath);
            continue;
        }

        // Check if module was already type-checked
        if (module->isTypeChecked) {
            std::cerr << "Module " << import->modulePath
                    << " already type-checked, reusing existing class info\n";

            // Import classes from the cached TypeChecker
            if (module->moduleTypeChecker) {
                for (const auto &[className, classInfo]:
                     module->moduleTypeChecker->getClassInfo()) {
                    // Only add if not already in our classes map
                    if (classes.find(className) == classes.end()) {
                        classes[className] = classInfo;
                    }
                }
            }

            // Import symbols
            importSymbolsFromModule(module, import.get());
            continue;
        }

        std::cerr << "Type-checking module " << import->modulePath << " for first time\n";

        // Type check the imported module
        auto moduleChecker = std::make_unique<TypeChecker>();
        moduleChecker->setModuleLoader(moduleLoader);
        moduleChecker->check(*module->ast);

        // Mark as type-checked and save the TypeChecker
        module->isTypeChecked = true;

        // Copy class info from module to our scope
        for (const auto &[className, classInfo]: moduleChecker->getClassInfo()) {
            // Only add if not already present
            if (classes.find(className) == classes.end()) {
                classes[className] = classInfo;
                std::cerr << "  Added class: " << className << "\n";
            } else {
                std::cerr << "  Skipped duplicate class: " << className << "\n";
            }
        }

        // Save the TypeChecker for future use
        module->moduleTypeChecker = std::move(moduleChecker);

        // Import symbols
        importSymbolsFromModule(module, import.get());
    }
}

void TypeChecker::importSymbolsFromModule(const ModuleInfo *module, const ImportStmt *import) {
    if (!module || !module->ast) return;

    // If wildcard import (import module.*)
    if (import->isWildcard || import->symbols.empty()) {
        // Import all functions
        for (const auto &func: module->ast->functions) {
            auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
            for (const auto &param: func->params) {
                funcType->paramTypes.push_back(resolveType(param.type));
            }
            funcType->returnType = func->returnType.empty()
                                       ? Type::Void()
                                       : resolveType(func->returnType);

            // Preserve the error return flag
            funcType->canReturnError = func->mayReturnError;

            Symbol symbol(Symbol::Kind::FUNCTION, func->name, funcType);
            globalScope->define(symbol);

            std::cerr << "  Imported function: " << func->name << "\n";
        }

        // Import all classes
        for (const auto &cls: module->ast->classes) {
            // Classes are already in the classes map from type checking
            std::cerr << "  Imported class: " << cls->name << "\n";
        }
    } else {
        // Import specific symbols
        for (const auto &symbolName: import->symbols) {
            bool found = false;

            // Look for function
            for (const auto &func: module->ast->functions) {
                // Import specific symbols (inside the symbolName loop)
                if (func->name == symbolName) {
                    auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
                    for (const auto &param: func->params) {
                        funcType->paramTypes.push_back(resolveType(param.type));
                    }
                    funcType->returnType = func->returnType.empty()
                                               ? Type::Void()
                                               : resolveType(func->returnType);

                    // Preserve the error return flag
                    funcType->canReturnError = func->mayReturnError;

                    Symbol symbol(Symbol::Kind::FUNCTION, func->name, funcType);
                    globalScope->define(symbol);

                    std::cerr << "  Imported function: " << func->name << "\n";
                    found = true;
                    break;
                }
            }

            // Look for class
            if (!found) {
                for (const auto &cls: module->ast->classes) {
                    if (cls->name == symbolName) {
                        std::cerr << "  Imported class: " << cls->name << "\n";
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                error("Symbol '" + symbolName + "' not found in module " + import->modulePath);
            }
        }
    }
}

void TypeChecker::registerBuiltinTypes() {
    // Register built-in list methods
    ClassInfo listClass;
    listClass.name = "list";
    listClass.typeParams.push_back("T");
    classes["list"] = listClass;

    // Register built-in dict methods
    ClassInfo dictClass;
    dictClass.name = "dict";
    dictClass.typeParams.push_back("K");
    dictClass.typeParams.push_back("V");
    classes["dict"] = dictClass;

    // Register built-in string methods
    ClassInfo strClass;
    strClass.name = "str";
    classes["str"] = strClass;
}

void TypeChecker::registerBuiltins() const {
    // exit(...) -> void
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->returnType = Type::Void();
        Symbol symbol(Symbol::Kind::FUNCTION, "exit", funcType);
        globalScope->define(symbol);
    }

    // print(...) -> void
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->returnType = Type::Void();
        Symbol symbol(Symbol::Kind::FUNCTION, "print", funcType);
        globalScope->define(symbol);
    }

    // println(...) -> void
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->returnType = Type::Void();
        Symbol symbol(Symbol::Kind::FUNCTION, "println", funcType);
        globalScope->define(symbol);
    }

    // android_native_toast(...) -> void
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->returnType = Type::Void();
        Symbol symbol(Symbol::Kind::FUNCTION, "android_native_toast", funcType);
        globalScope->define(symbol);
    }

    // str(value) -> str
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->paramTypes.push_back(Type::Unknown());
        funcType->returnType = Type::String();
        Symbol symbol(Symbol::Kind::FUNCTION, "str", funcType);
        globalScope->define(symbol);
    }

    // len(collection) -> int
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->paramTypes.push_back(Type::Unknown());
        funcType->returnType = Type::Int();
        Symbol symbol(Symbol::Kind::FUNCTION, "len", funcType);
        globalScope->define(symbol);
    }

    // int(value) -> int
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->paramTypes.push_back(Type::Unknown());
        funcType->returnType = Type::Int();
        Symbol symbol(Symbol::Kind::FUNCTION, "int", funcType);
        globalScope->define(symbol);
    }

    // float(value) -> float
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->paramTypes.push_back(Type::Unknown());
        funcType->returnType = Type::Float();
        Symbol symbol(Symbol::Kind::FUNCTION, "float", funcType);
        globalScope->define(symbol);
    }

    // input(...) -> str
    {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        funcType->returnType = Type::String();
        Symbol symbol(Symbol::Kind::FUNCTION, "input", funcType);
        globalScope->define(symbol);
    }
}

void TypeChecker::check(const Program &program) {
    // Initialize global scope
    globalScope = std::make_shared<Scope>();
    currentScope = globalScope.get();

    // Register built-in types and functions FIRST
    registerBuiltinTypes();
    registerBuiltins();

    if (moduleLoader) {
        processImports(program);
    }

    // Phase 1: Collect all class declarations
    for (auto &classDecl: program.classes) {
        analyzeClass(classDecl.get());
    }

    // Phase 2: Analyze class hierarchy and compute field offsets
    analyzeClassHierarchy();

    // Phase 3: Register global functions
    for (auto& func : program.functions) {
        auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
        for (const auto& param : func->params) {
            funcType->paramTypes.push_back(resolveType(param.type));
        }

        funcType->returnType = func->returnType.empty()
            ? Type::Void()
            : resolveType(func->returnType);

        // Manually set the error flag if the function may return error
        if (func->mayReturnError && funcType->returnType) {
            funcType->returnType->canReturnError = true;
            funcType->returnType->isChecked = false;
        }

        Symbol symbol(Symbol::Kind::FUNCTION, func->name, funcType);
        globalScope->define(symbol);
    }

    // Phase 4: Type check all functions
    for (auto &func: program.functions) {
        checkFunction(func.get());
    }

    // Phase 5: Type check all class methods
    for (auto &classDecl: program.classes) {
        currentClassName = classDecl->name;

        // Check constructor
        if (classDecl->constructor) {
            checkFunction(classDecl->constructor.get());
        }

        // Check methods
        for (auto &method: classDecl->methods) {
            checkFunction(method.get());
        }

        currentClassName.clear();
    }
}

void TypeChecker::analyzeClass(const ClassDecl *classDecl) {
    if (classes.find(classDecl->name) != classes.end()) {
        error("Class '" + classDecl->name + "' is already defined");
    }

    ClassInfo info;
    info.name = classDecl->name;
    info.parentClass = classDecl->parentClass;
    info.typeParams = classDecl->typeParams;
    info.isSealed = classDecl->isSealed;
    info.constructor = classDecl->constructor.get();

    // Collect fields (check for duplicates)
    for (const auto &field: classDecl->fields) {
        // Check if field already exists
        if (info.fields.find(field.name) != info.fields.end()) {
            error("Duplicate field '" + field.name + "' in class '" + classDecl->name + "'");
        }

        auto fieldType = resolveTypeWithGenerics(field.type, classDecl->typeParams);
        info.fields[field.name] = fieldType;
        info.fields[field.name]->visibility = field.visibility;
    }

    // Collect methods
    for (auto &method: classDecl->methods) {
        info.methods[method->name] = method.get();
    }

    classes[classDecl->name] = info;
}

void TypeChecker::analyzeClassHierarchy() {
    // Check for inheritance cycles
    for (auto &[className, classInfo]: classes) {
        std::string current = className;
        std::vector<std::string> visited;

        while (!current.empty()) {
            if (std::find(visited.begin(), visited.end(), current) != visited.end()) {
                error("Circular inheritance detected involving class '" + className + "'");
            }
            visited.push_back(current);

            auto it = classes.find(current);
            if (it == classes.end()) break;

            if (!it->second.parentClass.empty()) {
                auto parentIt = classes.find(it->second.parentClass);
                if (parentIt == classes.end()) {
                    error("Parent class '" + it->second.parentClass + "' not found");
                }
                if (parentIt->second.isSealed) {
                    error("Cannot inherit from sealed class '" + it->second.parentClass + "'");
                }
            }

            current = it->second.parentClass;
        }
    }

    // Compute field offsets
    for (auto &[className, classInfo]: classes) {
        // Only compute if not already computed
        if (classInfo.fieldOffsets.empty() || classInfo.totalFieldCount == 0) {
            computeFieldOffsets(classInfo);
        } else {
            std::cerr << "Skipping field offset computation for "
                    << className << " (already computed)\n";
        }
    }
}

void TypeChecker::computeFieldOffsets(ClassInfo &info) {
    // If already computed, skip
    if (info.totalFieldCount > 0) {
        return;
    }

    int offset = 0;

    // First, add parent fields
    if (!info.parentClass.empty()) {
        auto parentIt = classes.find(info.parentClass);
        if (parentIt != classes.end()) {
            // Make sure parent offsets are computed first
            if (parentIt->second.totalFieldCount == 0) {
                computeFieldOffsets(parentIt->second);
            }

            for (const auto &[fieldName, fieldType]: parentIt->second.fields) {
                info.fieldOffsets[fieldName] = offset++;
            }
        }
    }

    // Then add own fields
    for (const auto &[fieldName, fieldType]: info.fields) {
        // Check if field already has an offset (from parent or previous computation)
        if (info.fieldOffsets.find(fieldName) != info.fieldOffsets.end()) {
            // If it came from parent, that's shadowing - error
            if (!info.parentClass.empty()) {
                auto parentIt = classes.find(info.parentClass);
                if (parentIt != classes.end() &&
                    parentIt->second.fields.find(fieldName) != parentIt->second.fields.end()) {
                    error("Field '" + fieldName + "' shadows parent field in class '"
                          + info.name + "'");
                }
            }
            // Otherwise it's already computed, skip
            continue;
        }

        info.fieldOffsets[fieldName] = offset++;
    }

    info.totalFieldCount = offset;
}

std::shared_ptr<Type> TypeChecker::resolveType(const std::string& typeStr) {
    if (!typeStr.empty() && typeStr.back() == '!') {
        std::string baseType = typeStr.substr(0, typeStr.length() - 1);
        auto type = resolveTypeWithGenerics(baseType, {});
        type->canReturnError = true;
        type->isChecked = false;
        return type;
    }
    return resolveTypeWithGenerics(typeStr, {});
}

void TypeChecker::enforceErrorCheck(const std::string& varName, const std::shared_ptr<Type>& type) {
    if (isInIsErrorCheck) {
        return;
    }

    if (type->canReturnError && !type->isChecked) {
        error("Cannot use a possibly failing value of type " + type->toString() + " without handling the Error first. " + "Use 'if " + varName + " is Error { ... }' to check.");
    }
}

bool TypeChecker::blockDefinitelyReturns(const Stmt *stmt) {
    // Check if this is a return statement
    if (dynamic_cast<const ReturnStmt *>(stmt)) {
        return true;
    }

    // Check if this is an exit() call
    if (auto exprStmt = dynamic_cast<const ExprStmt *>(stmt)) {
        if (auto call = dynamic_cast<const CallExpr *>(exprStmt->expr.get())) {
            if (auto id = dynamic_cast<const IdentifierExpr *>(call->callee.get())) {
                if (id->name == "exit") {
                    return true;
                }
            }
        }
    }

    // Check if any statement in a block returns
    if (auto block = dynamic_cast<const BlockStmt *>(stmt)) {
        for (auto &s: block->statements) {
            if (blockDefinitelyReturns(s.get())) {
                return true;
            }
        }
    }

    return false;
}

std::shared_ptr<Type> TypeChecker::resolveTypeWithGenerics(const std::string &typeStr,
                                                           const std::vector<std::string> &typeParams) {
    // Handle generic type parameters
    if (std::ranges::find(typeParams, typeStr) != typeParams.end()) {
        return std::make_shared<Type>(Type::Kind::GENERIC, typeStr);
    }

    // Parse basic types
    if (typeStr == "int") return Type::Int();
    if (typeStr == "float") return Type::Float();
    if (typeStr == "bool") return Type::Bool();
    if (typeStr == "str") return Type::String();
    if (typeStr == "void") return Type::Void();
    if (typeStr == "null") return Type::Null();

    // Parse parametric types: list[T], dict[K,V]
    size_t bracketPos = typeStr.find('[');
    if (bracketPos != std::string::npos) {
        std::string baseName = typeStr.substr(0, bracketPos);
        size_t endBracket = typeStr.find(']', bracketPos);
        std::string paramsStr = typeStr.substr(bracketPos + 1, endBracket - bracketPos - 1);

        if (baseName == "list") {
            auto elemType = resolveTypeWithGenerics(paramsStr, typeParams);
            return Type::List(elemType);
        } else if (baseName == "dict") {
            // Parse "K,V"
            size_t commaPos = paramsStr.find(',');
            std::string keyTypeStr = paramsStr.substr(0, commaPos);
            std::string valTypeStr = paramsStr.substr(commaPos + 1);
            auto keyType = resolveTypeWithGenerics(keyTypeStr, typeParams);
            auto valType = resolveTypeWithGenerics(valTypeStr, typeParams);
            return Type::Dict(keyType, valType);
        }
    }

    // Check if it's a class type
    if (classes.contains(typeStr)) {
        return Type::Object(typeStr);
    }

    // Unknown type
    return Type::Unknown();
}

void TypeChecker::checkFunction(const FunctionDecl *func) {
    enterScope();

    // Set current return type
    currentFunctionReturnType = func->returnType.empty()
                                    ? Type::Void()
                                    : resolveType(func->returnType);

    if (func->mayReturnError && currentFunctionReturnType) {
        currentFunctionReturnType->canReturnError = true;
        currentFunctionReturnType->isChecked = false;
    }

    currentFunctionMayReturnError = func->mayReturnError;

    // Add 'self' parameter if this is a method
    if (!currentClassName.empty() && !func->isStatic) {
        auto selfType = Type::Object(currentClassName);
        Symbol selfSymbol(Symbol::Kind::PARAMETER, "self", selfType);
        currentScope->define(selfSymbol);
    }

    // Add parameters to scope
    for (const auto &param: func->params) {
        auto paramType = resolveType(param.type);
        Symbol symbol(Symbol::Kind::PARAMETER, param.name, paramType);
        currentScope->define(symbol);
    }

    // Check function body
    if (func->body) {
        checkStmt(func->body.get());
    }

    exitScope();
}

void TypeChecker::checkFieldDecl(const FieldDecl &field) {
    auto fieldType = resolveType(field.type);

    if (field.initializer) {
        auto initType = checkExpr(field.initializer.get());
        if (!isAssignable(fieldType, initType)) {
            error("Field '" + field.name + "' initializer type mismatch: expected " +
                  fieldType->toString() + ", got " + initType->toString());
        }
    }
}

void TypeChecker::checkStmt(Stmt *stmt) {
    if (auto varDecl = dynamic_cast<VarDeclStmt *>(stmt)) {
        checkVarDecl(varDecl);
    } else if (auto block = dynamic_cast<BlockStmt *>(stmt)) {
        checkBlock(block);
    } else if (auto ifStmt = dynamic_cast<IfStmt *>(stmt)) {
        checkIf(ifStmt);
    } else if (auto whileStmt = dynamic_cast<WhileStmt *>(stmt)) {
        checkWhile(whileStmt);
    } else if (auto forStmt = dynamic_cast<ForStmt *>(stmt)) {
        checkFor(forStmt);
    } else if (auto loopStmt = dynamic_cast<LoopStmt *>(stmt)) {
        checkLoop(loopStmt);
    } else if (auto returnStmt = dynamic_cast<ReturnStmt *>(stmt)) {
        checkReturn(returnStmt);
    } else if (auto exprStmt = dynamic_cast<ExprStmt *>(stmt)) {
        checkExprStmt(exprStmt);
    }
}

void TypeChecker::checkVarDecl(const VarDeclStmt* stmt) {
    std::shared_ptr<Type> varType;

    if (!stmt->type.empty()) {
        varType = resolveType(stmt->type);
    }

    if (stmt->initializer) {
        auto initType = checkExpr(stmt->initializer.get());  // ← This calls checkCall

        if (varType) {
            if (!isAssignable(varType, initType)) {
                error("Variable '" + stmt->name + "' type mismatch: expected " +
                      varType->toString() + ", got " + initType->toString());
            }
        } else {
            varType = initType;  // ← The variable gets the return type from the function
        }
    } else if (!varType) {
        error("Variable '" + stmt->name + "' must have type annotation or initializer");
    }

    Symbol symbol(Symbol::Kind::VARIABLE, stmt->name, varType);
    currentScope->define(symbol);
}

void TypeChecker::checkBlock(const BlockStmt *stmt) {
    enterScope();
    for (auto &statement: stmt->statements) {
        checkStmt(statement.get());
    }
    exitScope();
}

void TypeChecker::checkIf(const IfStmt *stmt) {
    auto condType = checkExpr(stmt->condition.get());
    if (condType->kind != Type::Kind::BOOL) {
        error("If condition must be bool, got " + condType->toString());
    }

    // Check if condition is "variable is Error" for type narrowing
    std::string narrowedVar;
    bool isErrorCheck = false;

    if (auto isExpr = dynamic_cast<IsExpr *>(stmt->condition.get())) {
        if (auto id = dynamic_cast<IdentifierExpr *>(isExpr->expr.get())) {
            if (isExpr->targetType == "Error") {
                narrowedVar = id->name;
                isErrorCheck = true;
            }
        }
    }

    // Check THEN branch with type narrowing
    enterScope();
    if (isErrorCheck && !narrowedVar.empty()) {
        Symbol *originalSymbol = currentScope->parent->resolve(narrowedVar);
        if (originalSymbol && originalSymbol->type->canReturnError) {
            // In then-branch: variable IS Error
            auto errorType = Type::Object("Error");
            Symbol narrowedSymbol(Symbol::Kind::VARIABLE, narrowedVar, errorType);
            currentScope->define(narrowedSymbol);
        }
    }
    checkStmt(stmt->thenBranch.get());
    bool thenReturns = blockDefinitelyReturns(stmt->thenBranch.get());
    exitScope();

    // Check ELSE branch with opposite narrowing
    if (stmt->elseBranch) {
        enterScope();
        if (isErrorCheck && !narrowedVar.empty()) {  // ← FIXED: Removed the wrong condition
            Symbol *originalSymbol = currentScope->parent->resolve(narrowedVar);
            if (originalSymbol && originalSymbol->type->canReturnError) {
                // In else-branch: variable is NOT Error (unwrapped)
                auto unwrappedType = std::make_shared<Type>(*originalSymbol->type);
                unwrappedType->canReturnError = false;
                unwrappedType->isChecked = true;
                Symbol unwrappedSymbol(Symbol::Kind::VARIABLE, narrowedVar, unwrappedType);
                currentScope->define(unwrappedSymbol);
            }
        }
        checkStmt(stmt->elseBranch.get());
        exitScope();
    }

    // Guard pattern support: if the "is Error" branch definitely returns,
    // unwrap the variable after the if statement
    if (isErrorCheck && !narrowedVar.empty() && thenReturns && !stmt->elseBranch) {
        Symbol *originalSymbol = currentScope->resolve(narrowedVar);
        if (originalSymbol && originalSymbol->type->canReturnError) {
            // Unwrap in current scope
            auto unwrappedType = std::make_shared<Type>(*originalSymbol->type);
            unwrappedType->canReturnError = false;
            unwrappedType->isChecked = true;
            Symbol unwrappedSymbol(Symbol::Kind::VARIABLE, narrowedVar, unwrappedType);
            currentScope->define(unwrappedSymbol);
        }
    }
}

void TypeChecker::checkWhile(const WhileStmt *stmt) {
    auto condType = checkExpr(stmt->condition.get());
    if (condType->kind != Type::Kind::BOOL) {
        error("While condition must be bool, got " + condType->toString());
    }

    checkStmt(stmt->body.get());
}

void TypeChecker::checkFor(const ForStmt *stmt) {
    auto iterType = checkExpr(stmt->iterable.get());

    // Extract element type from iterable
    std::shared_ptr<Type> elemType;
    if (iterType->kind == Type::Kind::LIST) {
        elemType = iterType->typeParams[0];
    } else {
        error("For loop requires iterable type, got " + iterType->toString());
    }

    enterScope();
    Symbol loopVar(Symbol::Kind::VARIABLE, stmt->variable, elemType);
    currentScope->define(loopVar);
    checkStmt(stmt->body.get());
    exitScope();
}

void TypeChecker::checkLoop(const LoopStmt *stmt) {
    checkStmt(stmt->body.get());
}

void TypeChecker::checkReturn(const ReturnStmt *stmt) {
    if (stmt->value) {
        auto returnType = checkExpr(stmt->value.get());

        if (!isAssignable(currentFunctionReturnType, returnType)) {
            // Check if returning Error when function signature allows it
            if (currentFunctionMayReturnError && returnType->kind == Type::Kind::OBJECT) {
                // Check if it's an Error or Error subclass
                if (returnType->className == "Error" || isSubclass(returnType->className, "Error")) {
                    return; // Valid error return
                }
            }

            error("Return type mismatch: expected " +
                  currentFunctionReturnType->toString() + ", got " +
                  returnType->toString());
        }
    } else {
        if (currentFunctionReturnType->kind != Type::Kind::VOID) {
            error("Function must return " + currentFunctionReturnType->toString());
        }
    }
}

void TypeChecker::checkExprStmt(const ExprStmt *stmt) {
    checkExpr(stmt->expr.get());
}

std::shared_ptr<Type> TypeChecker::checkExpr(Expr *expr) {
    std::shared_ptr<Type> type;

    if (auto lit = dynamic_cast<LiteralExpr *>(expr)) {
        type = checkLiteral(lit);
    } else if (auto id = dynamic_cast<IdentifierExpr *>(expr)) {
        type = checkIdentifier(id);
    } else if (auto bin = dynamic_cast<BinaryExpr *>(expr)) {
        type = checkBinary(bin);
    } else if (auto un = dynamic_cast<UnaryExpr *>(expr)) {
        type = checkUnary(un);
    } else if (auto assign = dynamic_cast<AssignExpr *>(expr)) {
        type = checkAssign(assign);
    } else if (auto compAssign = dynamic_cast<CompoundAssignExpr *>(expr)) {
        type = checkCompoundAssign(compAssign);
    } else if (auto call = dynamic_cast<CallExpr *>(expr)) {
        type = checkCall(call);
    } else if (auto field = dynamic_cast<FieldAccessExpr *>(expr)) {
        type = checkFieldAccess(field);
    } else if (auto index = dynamic_cast<IndexExpr *>(expr)) {
        type = checkIndex(index);
    } else if (auto newExpr = dynamic_cast<NewExpr *>(expr)) {
        type = checkNew(newExpr);
    } else if (auto list = dynamic_cast<ListExpr *>(expr)) {
        type = checkList(list);
    } else if (auto dict = dynamic_cast<DictExpr *>(expr)) {
        type = checkDict(dict);
    } else if (auto cast = dynamic_cast<CastExpr *>(expr)) {
        type = checkCast(cast);
    } else if (auto isExpr = dynamic_cast<IsExpr *>(expr)) {
        type = checkIs(isExpr);
    } else {
        type = Type::Unknown();
    }

    // Store the inferred type on the expression for code generation
    expr->type = type;

    return type;
}

std::shared_ptr<Type> TypeChecker::checkLiteral(const LiteralExpr *expr) {
    switch (expr->type) {
        case LiteralExpr::Type::INT:
            return Type::Int();
        case LiteralExpr::Type::FLOAT:
            return Type::Float();
        case LiteralExpr::Type::BOOL:
            return Type::Bool();
        case LiteralExpr::Type::STRING:
            return Type::String();
        case LiteralExpr::Type::NULLVAL:
            return Type::Null();
    }
    return Type::Unknown();
}

std::shared_ptr<Type> TypeChecker::checkIdentifier(const IdentifierExpr *expr) {
    Symbol *symbol = currentScope->resolve(expr->name);
    if (!symbol) {
        error("Undefined variable '" + expr->name + "'");
    }
    enforceErrorCheck(expr->name, symbol->type);
    return symbol->type;
}

std::shared_ptr<Type> TypeChecker::checkBinary(BinaryExpr *expr) {
    auto leftType = checkExpr(expr->left.get());
    auto rightType = checkExpr(expr->right.get());

    // Can't use error types in binary operations without checking
    if (auto leftId = dynamic_cast<IdentifierExpr *>(expr->left.get())) {
        enforceErrorCheck(leftId->name, leftType);
    }
    if (auto rightId = dynamic_cast<IdentifierExpr *>(expr->right.get())) {
        enforceErrorCheck(rightId->name, rightType);
    }

    // --- Step 1: Operator overloading check ---
    if (leftType->kind == Type::Kind::OBJECT) {
        auto classIt = classes.find(leftType->className);
        if (classIt != classes.end()) {
            auto &cls = classIt->second;

            // Map enum -> common symbol and word forms
            std::string symbol;
            std::string word;
            switch (expr->op) {
                case BinaryExpr::Op::ADD: symbol = "+";
                    word = "add";
                    break;
                case BinaryExpr::Op::SUB: symbol = "-";
                    word = "sub";
                    break;
                case BinaryExpr::Op::MUL: symbol = "*";
                    word = "mul";
                    break;
                case BinaryExpr::Op::DIV: symbol = "/";
                    word = "div";
                    break;
                case BinaryExpr::Op::MOD: symbol = "%";
                    word = "mod";
                    break;
                case BinaryExpr::Op::EQ: symbol = "==";
                    word = "eq";
                    break;
                case BinaryExpr::Op::NEQ: symbol = "!=";
                    word = "neq";
                    break;
                case BinaryExpr::Op::LT: symbol = "<";
                    word = "lt";
                    break;
                case BinaryExpr::Op::LTE: symbol = "<=";
                    word = "lte";
                    break;
                case BinaryExpr::Op::GT: symbol = ">";
                    word = "gt";
                    break;
                case BinaryExpr::Op::GTE: symbol = ">=";
                    word = "gte";
                    break;
                default: break;
            }

            if (!symbol.empty()) {
                std::vector<std::string> candidates;
                candidates.push_back("op " + symbol);
                candidates.push_back("op" + symbol);
                candidates.push_back("operator" + symbol);

                if (!word.empty()) {
                    candidates.push_back("op " + word);
                    candidates.push_back("op" + word);
                    candidates.push_back("op_" + word);
                    candidates.push_back("op$" + word);
                    candidates.push_back("operator" + word);
                }

                // Try all candidates
                for (const auto &name: candidates) {
                    auto it = cls.methods.find(name);
                    if (it != cls.methods.end()) {
                        FunctionDecl *method = it->second;

                        if (method->params.size() != 1) {
                            error("Operator '" + name + "' in class '" + leftType->className +
                                  "' must have exactly one parameter");
                            return Type::Unknown();
                        }

                        auto paramType = resolveType(method->params[0].type);
                        if (!isAssignable(paramType, rightType)) {
                            error("Operator '" + name + "' expects right operand of type " +
                                  paramType->toString() + ", got " + rightType->toString());
                            return Type::Unknown();
                        }

                        // IMPORTANT: Mark that this expression uses operator overloading
                        expr->hasOperatorOverload = true;
                        expr->operatorMethodName = name;

                        return method->returnType.empty()
                                   ? Type::Void()
                                   : resolveType(method->returnType);
                    }
                }
            }
        }
    }

    // --- Step 2: Fallback to built-in behavior (existing logic) ---
    switch (expr->op) {
        case BinaryExpr::Op::ADD:
        case BinaryExpr::Op::SUB:
        case BinaryExpr::Op::MUL:
        case BinaryExpr::Op::DIV:
        case BinaryExpr::Op::MOD:
            if (leftType->isNumeric() && rightType->isNumeric()) {
                return promoteNumeric(leftType, rightType);
            }
            // String concatenation for ADD
            if (expr->op == BinaryExpr::Op::ADD &&
                leftType->kind == Type::Kind::STRING &&
                rightType->kind == Type::Kind::STRING) {
                return Type::String();
            }
            error("Invalid operands for arithmetic operation");
            break;

        case BinaryExpr::Op::EQ:
        case BinaryExpr::Op::NEQ:
            return Type::Bool();

        case BinaryExpr::Op::LT:
        case BinaryExpr::Op::LTE:
        case BinaryExpr::Op::GT:
        case BinaryExpr::Op::GTE:
            if (leftType->isNumeric() && rightType->isNumeric()) {
                return Type::Bool();
            }
            error("Comparison operators require numeric types");
            break;

        case BinaryExpr::Op::AND:
        case BinaryExpr::Op::OR:
            if (leftType->kind == Type::Kind::BOOL && rightType->kind == Type::Kind::BOOL) {
                return Type::Bool();
            }
            error("Logical operators require bool types");
            break;
    }

    return Type::Unknown();
}

std::shared_ptr<Type> TypeChecker::checkUnary(const UnaryExpr *expr) {
    auto operandType = checkExpr(expr->operand.get());

    switch (expr->op) {
        case UnaryExpr::Op::NEG:
            if (operandType->isNumeric()) {
                return operandType;
            }
            error("Unary negation requires numeric type");
            break;

        case UnaryExpr::Op::NOT:
            if (operandType->kind == Type::Kind::BOOL) {
                return Type::Bool();
            }
            error("Logical not requires bool type");
            break;
    }

    return Type::Unknown();
}

std::shared_ptr<Type> TypeChecker::checkAssign(const AssignExpr *expr) {
    auto targetType = checkExpr(expr->target.get());
    auto valueType = checkExpr(expr->value.get());

    if (!isAssignable(targetType, valueType)) {
        error("Assignment type mismatch: cannot assign " +
              valueType->toString() + " to " + targetType->toString());
    }

    return valueType;
}

std::shared_ptr<Type> TypeChecker::checkCompoundAssign(const CompoundAssignExpr *expr) {
    auto targetType = checkExpr(expr->target.get());
    auto valueType = checkExpr(expr->value.get());

    if (!targetType->isNumeric() || !valueType->isNumeric()) {
        error("Compound assignment requires numeric types");
    }

    return targetType;
}

std::shared_ptr<Type> TypeChecker::checkFieldAccess(const FieldAccessExpr *expr) {
    auto objectType = checkExpr(expr->object.get());

    // Can't access fields on unchecked error types
    if (auto id = dynamic_cast<IdentifierExpr *>(expr->object.get())) {
        enforceErrorCheck(id->name, objectType);
    }

    // this will also respect the access modifier of field
    // pub -> everywhere
    // prot -> same class + inh class
    // priv -> only same class
    if (objectType->kind == Type::Kind::OBJECT) {
        // Start searching from the object's class
        std::string currentClass = objectType->className;

        // Search through class hierarchy
        while (!currentClass.empty()) {
            auto it = classes.find(currentClass);
            if (it == classes.end()) {
                break;
            }

            // Check fields in current class
            auto fieldIt = it->second.fields.find(expr->field);
            if (fieldIt != it->second.fields.end()) {
                // check visibility and throw from here?
                if (fieldIt->second->visibility == FieldDecl::Visibility::PRIVATE && currentClassName != currentClass) {
                    // only this class
                    error("Class '" + objectType->className + "' has no field or method '" + expr->field + "'");
                } else if (fieldIt->second->visibility == FieldDecl::Visibility::PROTECTED) {
                    // this and children
                    if (!isDescendant(currentClassName, currentClass, classes)) {
                        error("Class '" + objectType->className + "' has no field or method '" + expr->field + "'");
                    }
                }

                return fieldIt->second;
            }

            // Check methods in current class
            auto methodIt = it->second.methods.find(expr->field);
            if (methodIt != it->second.methods.end()) {
                // check visibility and throw from here?
                if (methodIt->second->visibility == FunctionDecl::Visibility::PRIVATE && currentClassName !=
                    currentClass) {
                    // only this class
                    error("Class '" + objectType->className + "' has no field or method '" + expr->field + "'");
                } else if (methodIt->second->visibility == FunctionDecl::Visibility::PROTECTED) {
                    // this and children
                    if (!isDescendant(currentClassName, currentClass, classes)) {
                        error("Class '" + objectType->className + "' has no field or method '" + expr->field + "'");
                    }
                }

                // Return function type for method reference
                auto funcType = std::make_shared<Type>(Type::Kind::FUNCTION);
                return funcType;
            }

            currentClass = it->second.parentClass;
        }

        // Not found in any class in the hierarchy
        error("Class '" + objectType->className + "' has no field or method '" + expr->field + "'");
    }

    return Type::Unknown();
}

std::shared_ptr<Type> TypeChecker::checkCall(const CallExpr *expr) {
    // If callee is a field access like X.foo(...)
    if (auto fieldAccess = dynamic_cast<FieldAccessExpr *>(expr->callee.get())) {
        // --- 1. STATIC METHOD CALL: ClassName.methodName() ---
        if (auto classId = dynamic_cast<IdentifierExpr *>(fieldAccess->object.get())) {
            auto classIt = classes.find(classId->name);
            if (classIt != classes.end()) {
                // Found a class with this name -> treat as static method call
                auto methodIt = classIt->second.methods.find(fieldAccess->field);
                if (methodIt != classIt->second.methods.end()) {
                    FunctionDecl *method = methodIt->second;

                    if (!method->isStatic) {
                        error("Cannot call non-static method '" + fieldAccess->field +
                              "' on class '" + classId->name + "'");
                        return Type::Unknown();
                    }

                    // Check argument count
                    if (expr->arguments.size() != method->params.size()) {
                        error("Static method '" + fieldAccess->field + "' expects " +
                              std::to_string(method->params.size()) + " arguments, got " +
                              std::to_string(expr->arguments.size()));
                        return Type::Unknown();
                    }

                    // Check argument types
                    for (size_t i = 0; i < expr->arguments.size(); ++i) {
                        auto argType = checkExpr(expr->arguments[i].get());
                        auto paramType = resolveType(method->params[i].type);
                        if (!isAssignable(paramType, argType)) {
                            error("Argument " + std::to_string(i + 1) + " type mismatch: expected " + paramType->
                                  toString() + ", got " + argType->toString());
                            return Type::Unknown();
                        }
                    }

                    auto returnType = method->returnType.empty() ? Type::Void() : resolveType(method->returnType);

                    if (method->mayReturnError && returnType) {
                        returnType->canReturnError = true;
                        returnType->isChecked = false;
                    }

                    return returnType;
                }

                error("Class '" + classId->name + "' has no static method '" + fieldAccess->field + "'");
                return Type::Unknown();
            }
        }

        // --- 2. INSTANCE METHOD CALL: obj.method() ---
        auto objectType = checkExpr(fieldAccess->object.get());
        if (objectType->kind == Type::Kind::OBJECT) {
            std::string currentClass = objectType->className;

            while (!currentClass.empty()) {
                auto it = classes.find(currentClass);
                if (it == classes.end()) break;

                auto methodIt = it->second.methods.find(fieldAccess->field);
                if (methodIt != it->second.methods.end()) {
                    FunctionDecl *method = methodIt->second;

                    if (expr->arguments.size() != method->params.size()) {
                        error("Method '" + fieldAccess->field + "' expects " +
                              std::to_string(method->params.size()) + " arguments, got " +
                              std::to_string(expr->arguments.size()));
                        return Type::Unknown();
                    }

                    for (size_t i = 0; i < expr->arguments.size(); ++i) {
                        // args are passed value
                        auto argType = checkExpr(expr->arguments[i].get());

                        // params are asked vars
                        const auto paramTypeStr = method->params[i].type;
                        auto paramType = resolveType(paramTypeStr);

                        if (!isAssignable(paramType, argType)) {
                            error("Argument " + std::to_string(i + 1) + " type mismatch: expected " + paramTypeStr +
                                  ", got " + argType->toString());
                            return Type::Unknown();
                        }
                    }

                    FunctionDecl::Visibility visibility = method->visibility;

                    if (visibility == FunctionDecl::Visibility::PRIVATE) {
                        // can be called from method of same class
                        if (currentClassName != currentClass) {
                            error("Private method can only be called from inside its own class.");
                            return Type::Unknown();
                        }
                    }
                    if (visibility == FunctionDecl::Visibility::PROTECTED) {
                        // can be called from the method of class which is inh of this class
                        if (currentClassName != currentClass) {
                            if (!isDescendant(currentClassName, currentClass, classes)) {
                                error("Protected method can only be called from its own child class or itself.");
                                return Type::Unknown();
                            }
                        }
                    }

                    auto returnType = method->returnType.empty() ? Type::Void() : resolveType(method->returnType);

                    if (method->mayReturnError && returnType) {
                        returnType->canReturnError = true;
                        returnType->isChecked = false;
                    }

                    return returnType;
                }

                currentClass = it->second.parentClass;
            }

            error("Class '" + objectType->className + "' has no method '" + fieldAccess->field + "'");
            return Type::Unknown();
        }

        error("Cannot call method on non-object type");
        return Type::Unknown();
    }

    // --- 3. SIMPLE IDENTIFIER CALL (e.g., foo(...)) ---
    else if (auto id = dynamic_cast<IdentifierExpr *>(expr->callee.get())) {
        // (a) CONSTRUCTOR CALL: ClassName(...)
        auto classIt = classes.find(id->name);
        if (classIt != classes.end()) {
            return Type::Object(id->name);
        }

        // (b) BUILT-IN FUNCTIONS
        if (
            id->name == "exit" ||
            id->name == "print" ||
            id->name == "println" ||
            id->name == "str" ||
            id->name == "len" ||
            id->name == "int" ||
            id->name == "float" ||
            id->name == "input" ||
            id->name == "android_native_toast"
        ) {
            for (auto &arg: expr->arguments) checkExpr(arg.get());
            return Type::Void();
        }

        // (c) REGULAR USER-DEFINED FUNCTION
        if (auto symbol = currentScope->resolve(id->name)) {
            if (symbol->kind == Symbol::Kind::FUNCTION &&
                symbol->type->kind == Type::Kind::FUNCTION) {
                // Check argument count
                if (expr->arguments.size() != symbol->type->paramTypes.size()) {
                    error("Function '" + id->name + "' expects " +
                          std::to_string(symbol->type->paramTypes.size()) + " arguments, got " +
                          std::to_string(expr->arguments.size()));
                    return Type::Unknown();
                }

                // Check argument types
                for (size_t i = 0; i < expr->arguments.size(); ++i) {
                    auto argType = checkExpr(expr->arguments[i].get());
                    if (!isAssignable(symbol->type->paramTypes[i], argType)) {
                        error("Argument " + std::to_string(i + 1) + " type mismatch in call to '" + id->name + "'");
                        return Type::Unknown();
                    }
                }

                auto returnType = symbol->type->returnType;
                return symbol->type->returnType;
            }
        }

        // (d) FFI FUNCTION HANDLING
        auto ffiIt = ffiFunctions.find(id->name);
        if (ffiIt != ffiFunctions.end()) {
            const auto &ffi = ffiIt->second;

            // Check argument count
            if (expr->arguments.size() != ffi.paramTypes.size()) {
                error("FFI function '" + id->name + "' expects " +
                      std::to_string(ffi.paramTypes.size()) + " arguments, got " +
                      std::to_string(expr->arguments.size()));
                return Type::Unknown();
            }

            // Check argument types
            for (size_t i = 0; i < expr->arguments.size(); ++i) {
                auto argType = checkExpr(expr->arguments[i].get());
                if (!isAssignable(ffi.paramTypes[i], argType)) {
                    error("Argument " + std::to_string(i + 1) + " type mismatch in FFI call '" +
                          id->name + "'");
                    return Type::Unknown();
                }
            }

            return ffi.returnType;
        }

        // (e) Otherwise, undefined
        error("Undefined function '" + id->name + "'");
        return Type::Unknown();
    }

    // --- Fallback ---
    return Type::Unknown();
}

bool TypeChecker::isDescendant(const std::string &childName, const std::string &potentialAncestor,
                               const std::unordered_map<std::string, ClassInfo> &classes) {
    std::string current = childName;
    std::unordered_set<std::string> visited;

    while (true) {
        // Detect circular inheritance
        if (visited.contains(current)) {
            std::cerr << "[Warning] Circular inheritance detected at class: " << current << "\n";
            return false;
        }

        visited.insert(current);

        auto it = classes.find(current);
        if (it == classes.end()) return false; // class not found
        const std::string &parent = it->second.parentClass;
        if (parent.empty()) return false; // no parent, stop
        if (parent == potentialAncestor) return true; // found ancestor
        current = parent; // move up the chain
    }

    return false;
}

std::shared_ptr<Type> TypeChecker::checkIndex(const IndexExpr *expr) {
    auto objectType = checkExpr(expr->object.get());
    auto indexType = checkExpr(expr->index.get());

    if (objectType->kind == Type::Kind::LIST) {
        if (indexType->kind != Type::Kind::INT) {
            error("List index must be int");
        }
        return objectType->typeParams[0];
    } else if (objectType->kind == Type::Kind::DICT) {
        if (!isAssignable(objectType->typeParams[0], indexType)) {
            error("Dict key type mismatch");
        }
        return objectType->typeParams[1];
    }

    error("Index operation requires list or dict type");
    return Type::Unknown();
}

std::shared_ptr<Type> TypeChecker::checkNew(const NewExpr *expr) {
    auto it = classes.find(expr->className);
    if (it == classes.end()) {
        error("Unknown class '" + expr->className + "'");
        return Type::Unknown();
    }

    const ClassInfo &classInfo = it->second;

    // Check constructor arguments
    if (classInfo.constructor) {
        if (expr->arguments.size() != classInfo.constructor->params.size()) {
            error("Constructor expects " +
                  std::to_string(classInfo.constructor->params.size()) +
                  " arguments, got " + std::to_string(expr->arguments.size()));
            return Type::Object(expr->className);
        }

        for (size_t i = 0; i < expr->arguments.size(); i++) {
            auto argType = checkExpr(expr->arguments[i].get());
            auto paramType = resolveType(classInfo.constructor->params[i].type);
            if (!isAssignable(paramType, argType)) {
                error("Constructor argument " + std::to_string(i + 1) + " type mismatch: expected " +
                      paramType->toString() + ", got " + argType->toString());
                return Type::Object(expr->className);
            }
        }
    } else if (!expr->arguments.empty()) {
        error("Class '" + expr->className + "' has no constructor but arguments were provided");
        return Type::Object(expr->className);
    }

    return Type::Object(expr->className);
}

std::shared_ptr<Type> TypeChecker::checkList(const ListExpr *expr) {
    if (expr->elements.empty()) {
        return Type::List(Type::Unknown());
    }

    auto firstType = checkExpr(expr->elements[0].get());

    for (size_t i = 1; i < expr->elements.size(); i++) {
        auto elemType = checkExpr(expr->elements[i].get());
        if (!elemType->equals(firstType)) {
            error("List elements must have uniform type");
        }
    }

    return Type::List(firstType);
}

std::shared_ptr<Type> TypeChecker::checkDict(const DictExpr *expr) {
    if (expr->pairs.empty()) {
        return Type::Dict(Type::Unknown(), Type::Unknown());
    }

    auto firstKeyType = checkExpr(expr->pairs[0].first.get());
    auto firstValType = checkExpr(expr->pairs[0].second.get());

    for (size_t i = 1; i < expr->pairs.size(); i++) {
        auto keyType = checkExpr(expr->pairs[i].first.get());
        auto valType = checkExpr(expr->pairs[i].second.get());

        if (!keyType->equals(firstKeyType)) {
            error("Dictionary keys must have uniform type");
        }
        if (!valType->equals(firstValType)) {
            error("Dictionary values must have uniform type");
        }
    }

    return Type::Dict(firstKeyType, firstValType);
}

std::shared_ptr<Type> TypeChecker::checkCast(const CastExpr *expr) {
    checkExpr(expr->expr.get());
    auto type = resolveType(expr->targetType);

    if (expr->expr->type->className == expr->targetType) {
        // converting same type to same type
        return type;
    }

    // not same type
    if (expr->expr->type->kind == Type::Kind::OBJECT) {
        // in case of object, either same or parent
        if (!isDescendant(expr->expr->type->className, expr->targetType, classes)) {
            error("Can not type convert type " + expr->expr->type->className + " to type " + expr->targetType);
        }
    }


    return type;
}

std::shared_ptr<Type> TypeChecker::checkIs(const IsExpr* expr) {
    // Temporarily disable error checking when evaluating the expression
    bool wasInIsErrorCheck = isInIsErrorCheck;
    isInIsErrorCheck = true;

    checkExpr(expr->expr.get());

    isInIsErrorCheck = wasInIsErrorCheck;

    return Type::Bool();
}

bool TypeChecker::isAssignable(const std::shared_ptr<Type> &target, const std::shared_ptr<Type> &source) {
    // Null can be assigned to any reference type
    if (source->kind == Type::Kind::NULL_TYPE &&
        (target->kind == Type::Kind::OBJECT ||
         target->kind == Type::Kind::LIST ||
         target->kind == Type::Kind::DICT)) {
        return true;
    }

    // Exact type match
    if (target->equals(source)) {
        return true;
    }

    // Numeric promotion
    if (target->kind == Type::Kind::FLOAT && source->kind == Type::Kind::INT) {
        return true;
    }

    // Inheritance check
    if (target->kind == Type::Kind::OBJECT && source->kind == Type::Kind::OBJECT) {
        return isSubclass(source->className, target->className);
    }

    return false;
}

bool TypeChecker::isSubclass(const std::string &child, const std::string &parent) {
    if (child == parent) return true;

    auto it = classes.find(child);
    if (it == classes.end()) return false;

    if (it->second.parentClass.empty()) return false;

    return isSubclass(it->second.parentClass, parent);
}

std::shared_ptr<Type> TypeChecker::promoteNumeric(const std::shared_ptr<Type> &a, const std::shared_ptr<Type> &b) {
    if (a->kind == Type::Kind::FLOAT || b->kind == Type::Kind::FLOAT) {
        return Type::Float();
    }
    return Type::Int();
}
