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
 *  File: CodeGenerator
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_CODEGENERATOR_H
#define DROPLET_CODEGENERATOR_H

#include "Expr.h"
#include "Program.h"
#include "Stmt.h"
#include "TypeChecker.h"
#include "../vm/DBC_helper.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

struct FunctionDebugInfo;

struct LocalVar {
    std::string name;
    uint8_t slot;
    int scopeDepth;
};

struct FunctionContext {
    DBCBuilder::FunctionBuilder* builder = nullptr;
    std::vector<LocalVar> locals;
    int scopeDepth = 0;
    uint8_t localCount = 0;
    std::string className;  // Empty for top-level functions
    bool isConstructor = false;

    void enterScope() { scopeDepth++; }

    void exitScope() {
        // Remove locals from this scope
        while (!locals.empty() && locals.back().scopeDepth >= scopeDepth) {
            locals.pop_back();
        }
        scopeDepth--;
    }

    uint8_t addLocal(const std::string& name) {
        LocalVar local;
        local.name = name;
        local.slot = localCount++;
        local.scopeDepth = scopeDepth;
        locals.push_back(local);
        return local.slot;
    }

    int findLocal(const std::string& name) const {
        for (int i = static_cast<int>(locals.size()) - 1; i >= 0; i--) {
            if (locals[i].name == name) {
                return locals[i].slot;
            }
        }
        return -1;
    }
};

struct LoopContext {
    std::vector<uint32_t> breakJumps;
    std::vector<uint32_t> continueJumps;
    uint32_t loopStart;
};

class CodeGenerator {
public:

    explicit CodeGenerator(const TypeChecker& typeChecker): typeChecker(typeChecker) {}

    // Main entry point
    bool generate(const Program& program, const std::string& outputPath);

    bool generateWithModules(const Program& mainProgram, const std::string& outputPath);

    void setGenerateDebugInfo(bool enable) { generateDebugInfo = enable; }
    void setSourceFile(const std::string& file) { currentSourceFile = file; }

    const std::map<uint32_t, FunctionDebugInfo>& getDebugInfo() const {
        return debugInfoMap;
    }

    void setModuleLoader(ModuleLoader* loader) {
        moduleLoader = loader;
    }

private:
    DBCBuilder builder;
    const TypeChecker& typeChecker;
    std::unordered_map<std::string, uint32_t> globalNames;  // Global variable name -> index
    std::unordered_map<std::string, uint32_t> functionIndices;  // Function name -> index
    std::unordered_map<std::string, uint32_t> stringConstants;  // String -> constant index
    FunctionContext* currentFunction = nullptr;
    std::vector<LoopContext> loopStack;
    ModuleLoader* moduleLoader = nullptr;
    std::unordered_map<std::string, FFIInfo*> ffiRegistry;

    bool generateDebugInfo = false;
    std::map<uint32_t, FunctionDebugInfo> debugInfoMap;
    std::string currentSourceFile;
    uint32_t currentFunctionIndex = 0;

    uint32_t lastRecordedLine = 0;
    uint32_t lastRecordedColumn = 0;

    void recordDebugLocation(DBCBuilder::FunctionBuilder& fb,  uint32_t line, uint32_t column);
    void finalizeDebugInfo(uint32_t funcIdx, const std::string& funcName);


    void registerFFI(const std::string& name, FFIInfo* decl) {
        ffiRegistry[name] = decl;
    }

    // Constant management
    uint32_t addStringConstant(const std::string& str);
    uint32_t getOrAddGlobal(const std::string& name);

    // Class code generation
    void generateClass(ClassDecl* classDecl);
    void generateConstructor(ClassDecl* classDecl);
    void generateMethod(FunctionDecl* method, const std::string& className);

    static std::string mangleName(const std::string& className, const std::string& methodName);

    // Function code generation
    void generateFunction(FunctionDecl* func, const std::string& mangledName = "");
    void generateFunctionBody(FunctionDecl* func, DBCBuilder::FunctionBuilder& fb);

    // Statement code generation
    void generateStmt(Stmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateVarDecl(VarDeclStmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateBlock(BlockStmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateIf(IfStmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateWhile(WhileStmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateFor(ForStmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateLoop(LoopStmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateReturn(ReturnStmt* stmt, DBCBuilder::FunctionBuilder& fb);
    void generateBreak(DBCBuilder::FunctionBuilder& fb);
    void generateContinue(DBCBuilder::FunctionBuilder& fb);
    void generateExprStmt(ExprStmt* stmt, DBCBuilder::FunctionBuilder& fb);

    // Expression code generation
    void generateExpr(Expr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateLiteral(LiteralExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateIdentifier(IdentifierExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateBinary(BinaryExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateUnary(UnaryExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateAssign(AssignExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateCompoundAssign(CompoundAssignExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateCall(CallExpr* expr, DBCBuilder::FunctionBuilder& fb);

    void debugPrintIndices();

    void generateFieldAccess(FieldAccessExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateIndex(IndexExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateNew(NewExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateList(ListExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateDict(DictExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateCast(CastExpr* expr, DBCBuilder::FunctionBuilder& fb);
    void generateIs(IsExpr* expr, DBCBuilder::FunctionBuilder& fb);

    // Helper methods
    void patchJump(DBCBuilder::FunctionBuilder& fb, uint32_t jumpPos, uint32_t target);
    Op getBinaryOp(BinaryExpr::Op op);
    bool isBuiltinFunction(const std::string& name);
    std::string getBuiltinFunctionName(const std::string& name);

    std::string findMethodInClass(const std::string &className, const std::string &methodName);
};

#endif