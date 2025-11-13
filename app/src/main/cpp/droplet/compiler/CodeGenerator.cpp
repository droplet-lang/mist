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
#include "CodeGenerator.h"
#include "../debugger/Debugger.h"

#include <algorithm>
#include <iostream>
#include <sstream>


bool CodeGenerator::generate(const Program& program, const std::string& outputPath) {
    // Phase 2: Generate all class constructors and methods
    // This also registers their indices
    for (auto& classDecl : program.classes) {
        generateClass(classDecl.get());
    }

    // Phase 3: Generate top-level functions
    for (auto& func : program.functions) {

        if (func->ffi.has_value()) {
            // this is ffi function
            const uint32_t nameIdx = addStringConstant(func->name);
            functionIndices[func->name] = nameIdx;
            registerFFI(func->name, &func->ffi.value());
        }

        generateFunction(func.get(), func->name);
    }

    // Phase 4: write everything to the dbc file
    return builder.writeToFile(outputPath);
}

bool CodeGenerator::generateWithModules(const Program& mainProgram, const std::string& outputPath) {
    // Phase 1: Generate code for all imported modules first
    if (moduleLoader) {
        for (const auto& [modulePath, module] : moduleLoader->getLoadedModules()) {
            if (!module->ast) continue;

            std::cerr << "Generating code for module: " << modulePath << "\n";

            // CRITICAL FIX: Set the source file for this module
            std::string previousSourceFile = currentSourceFile;
            currentSourceFile = module->filePath;

            std::cerr << "  Source file: " << currentSourceFile << "\n";

            // Generate classes
            for (auto& classDecl : module->ast->classes) {
                generateClass(classDecl.get());
            }

            // Generate functions
            for (auto& func : module->ast->functions) {
                if (func->ffi.has_value()) {
                    // this is ffi function
                    const uint32_t nameIdx = addStringConstant(func->name);
                    functionIndices[func->name] = nameIdx;
                    registerFFI(func->name, &func->ffi.value());
                }
                generateFunction(func.get(), func->name);
            }

            // Restore previous source file
            currentSourceFile = previousSourceFile;
        }
    }

    // Phase 2: Generate main program
    return generate(mainProgram, outputPath);
}

uint32_t CodeGenerator::addStringConstant(const std::string& str) {
    auto it = stringConstants.find(str);
    if (it != stringConstants.end()) {
        return it->second;
    }

    const uint32_t idx = builder.addConstString(str);
    stringConstants[str] = idx;
    return idx;
}

uint32_t CodeGenerator::getOrAddGlobal(const std::string& name) {
    auto it = globalNames.find(name);
    if (it != globalNames.end()) {
        return it->second;
    }

    uint32_t idx = addStringConstant(name);
    globalNames[name] = idx;
    return idx;
}


std::string CodeGenerator::mangleName(const std::string& className, const std::string& methodName) {
    return className + "$$" + methodName;
}

void CodeGenerator::generateClass(ClassDecl* classDecl) {
    // Generate constructor
    if (classDecl->constructor) {
        generateConstructor(classDecl);
    }

    // Generate all methods
    for (auto& method : classDecl->methods) {
        generateMethod(method.get(), classDecl->name);
    }

    // Generate static field initializers if needed
    for (const auto& field : classDecl->fields) {
        if (field.isStatic && field.initializer) {
            std::string staticFieldName = mangleName(classDecl->name, field.name);
            uint32_t globalIdx = getOrAddGlobal(staticFieldName);

            // Create a static initializer function
            auto& fb = builder.addFunction(staticFieldName + "$init");
            fb.setArgCount(0).setLocalCount(0);

            FunctionContext ctx;
            ctx.builder = &fb;
            currentFunction = &ctx;

            generateExpr(field.initializer.get(), fb);
            fb.storeGlobal(globalIdx);
            fb.ret(0);

            currentFunction = nullptr;
        }
    }
}

void CodeGenerator::generateConstructor(ClassDecl* classDecl) {
    std::string ctorName = mangleName(classDecl->name, "new");

    currentFunctionIndex = static_cast<uint32_t>(builder.functions.size());

    // Initialize debug info
    if (generateDebugInfo) {
        FunctionDebugInfo& info = debugInfoMap[currentFunctionIndex];
        info.name = ctorName;
        info.file = currentSourceFile;
    }

    functionIndices[ctorName] = currentFunctionIndex;

    auto& fb = builder.addFunction(ctorName);

    uint8_t paramCount = static_cast<uint8_t>(classDecl->constructor->params.size());
    fb.setArgCount(paramCount);

    FunctionContext ctx;
    ctx.builder = &fb;
    ctx.className = classDecl->name;
    ctx.isConstructor = true;
    currentFunction = &ctx;

    // Add constructor parameters to locals FIRST
    for (size_t i = 0; i < classDecl->constructor->params.size(); i++) {
        ctx.addLocal(classDecl->constructor->params[i].name);
    }

    // Create new object instance
    uint32_t classNameIdx = addStringConstant(classDecl->name);
    fb.newObject(classNameIdx);

    uint8_t selfSlot = ctx.addLocal("self");
    fb.storeLocal(selfSlot);

    ctx.localCount = static_cast<uint8_t>(ctx.locals.size());
    fb.setLocalCount(ctx.localCount);

    // Initialize ALL fields
    for (const auto& field : classDecl->fields) {
        if (!field.isStatic) {
            uint32_t fieldNameIdx = addStringConstant(field.name);
            fb.loadLocal(selfSlot);

            bool foundParam = false;
            for (size_t i = 0; i < classDecl->constructor->params.size(); i++) {
                if (classDecl->constructor->params[i].name == field.name) {
                    fb.loadLocal(static_cast<uint8_t>(i));
                    foundParam = true;
                    break;
                }
            }

            if (!foundParam) {
                if (field.initializer) {
                    generateExpr(field.initializer.get(), fb);
                } else {
                    fb.pushConst(builder.addConstNil());
                }
            }

            fb.setField(fieldNameIdx);
        }
    }

    // Generate constructor body (if any)
    if (classDecl->constructor->body) {
        auto* blockStmt = dynamic_cast<BlockStmt*>(classDecl->constructor->body.get());
        if (blockStmt) {
            for (auto& stmt : blockStmt->statements) {
                generateStmt(stmt.get(), fb);
            }
        }
    }

    fb.loadLocal(selfSlot);
    fb.ret(1);

    // Finalize debug info
    if (generateDebugInfo) {
        finalizeDebugInfo(currentFunctionIndex, ctorName);
    }

    currentFunction = nullptr;
}

void CodeGenerator::generateFunction(FunctionDecl* func, const std::string& mangledName) {
    std::string funcName = mangledName.empty() ? func->name : mangledName;

    // Get the current function count BEFORE adding
    currentFunctionIndex = static_cast<uint32_t>(builder.functions.size());

    // Initialize debug info for this function
    if (generateDebugInfo) {
        FunctionDebugInfo& info = debugInfoMap[currentFunctionIndex];
        info.name = funcName;
        info.file = currentSourceFile;
    }

    // Register FIRST
    functionIndices[funcName] = currentFunctionIndex;

    // Then add the function
    auto& fb = builder.addFunction(funcName);

    uint8_t paramCount = static_cast<uint8_t>(func->params.size());
    fb.setArgCount(paramCount);

    FunctionContext ctx;
    ctx.builder = &fb;
    currentFunction = &ctx;

    generateFunctionBody(func, fb);

    // Finalize debug info for this function
    if (generateDebugInfo) {
        finalizeDebugInfo(currentFunctionIndex, funcName);
    }

    currentFunction = nullptr;
}

void CodeGenerator::generateStmt(Stmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    if (generateDebugInfo && stmt->line > 0) {
        recordDebugLocation(fb, stmt->line, stmt->column);
    }

    if (auto varDecl = dynamic_cast<VarDeclStmt*>(stmt)) {
        generateVarDecl(varDecl, fb);
    } else if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
        generateBlock(block, fb);
    } else if (auto ifStmt = dynamic_cast<IfStmt*>(stmt)) {
        generateIf(ifStmt, fb);
    } else if (auto whileStmt = dynamic_cast<WhileStmt*>(stmt)) {
        generateWhile(whileStmt, fb);
    } else if (auto forStmt = dynamic_cast<ForStmt*>(stmt)) {
        generateFor(forStmt, fb);
    } else if (auto loopStmt = dynamic_cast<LoopStmt*>(stmt)) {
        generateLoop(loopStmt, fb);
    } else if (auto returnStmt = dynamic_cast<ReturnStmt*>(stmt)) {
        generateReturn(returnStmt, fb);
    } else if (dynamic_cast<BreakStmt*>(stmt)) {
        generateBreak(fb);
    } else if (dynamic_cast<ContinueStmt*>(stmt)) {
        generateContinue(fb);
    } else if (auto exprStmt = dynamic_cast<ExprStmt*>(stmt)) {
        generateExprStmt(exprStmt, fb);
    }
}

void CodeGenerator::generateVarDecl(VarDeclStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    uint8_t slot = currentFunction->addLocal(stmt->name);
    currentFunction->localCount = std::max(currentFunction->localCount,
                                           static_cast<uint8_t>(currentFunction->locals.size()));

    if (stmt->initializer) {
        generateExpr(stmt->initializer.get(), fb);
        fb.storeLocal(slot);
    } else {
        fb.pushConst(builder.addConstNil());
        fb.storeLocal(slot);
    }
}

void CodeGenerator::generateBlock(BlockStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    currentFunction->enterScope();

    for (auto& statement : stmt->statements) {
        generateStmt(statement.get(), fb);
    }

    currentFunction->exitScope();
}

void CodeGenerator::generateIf(IfStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    // Generate condition
    generateExpr(stmt->condition.get(), fb);

    // Jump to else if false
    uint32_t elseJump = fb.currentPos();
    fb.jumpIfFalse(0);  // Placeholder

    // Generate then branch
    generateStmt(stmt->thenBranch.get(), fb);

    if (stmt->elseBranch) {
        // Jump over else
        uint32_t endJump = fb.currentPos();
        fb.jump(0);  // Placeholder

        // Patch else jump
        uint32_t elseStart = fb.currentPos();
        patchJump(fb, elseJump + 1, elseStart);

        // Generate else branch
        generateStmt(stmt->elseBranch.get(), fb);

        // Patch end jump
        uint32_t endPos = fb.currentPos();
        patchJump(fb, endJump + 1, endPos);
    } else {
        // Patch else jump to end
        uint32_t endPos = fb.currentPos();
        patchJump(fb, elseJump + 1, endPos);
    }
}

void CodeGenerator::generateWhile(WhileStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    LoopContext loopCtx;
    loopCtx.loopStart = fb.currentPos();
    loopStack.push_back(loopCtx);

    // Generate condition
    generateExpr(stmt->condition.get(), fb);

    // Jump to end if false
    uint32_t exitJump = fb.currentPos();
    fb.jumpIfFalse(0);  // Placeholder

    // Generate body
    generateStmt(stmt->body.get(), fb);

    // Jump back to start
    fb.jump(loopCtx.loopStart);

    // Patch exit jump
    uint32_t endPos = fb.currentPos();
    patchJump(fb, exitJump + 1, endPos);

    // Patch break jumps
    for (uint32_t breakPos : loopStack.back().breakJumps) {
        patchJump(fb, breakPos, endPos);
    }

    // Patch continue jumps
    for (uint32_t continuePos : loopStack.back().continueJumps) {
        patchJump(fb, continuePos, loopCtx.loopStart);
    }

    loopStack.pop_back();
}

void CodeGenerator::generateFor(ForStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    currentFunction->enterScope();

    // Generate iterable
    generateExpr(stmt->iterable.get(), fb);

    // For now, assume it's a list and iterate over it
    // This is a simplified implementation
    // Store iterator variable
    uint8_t iterSlot = currentFunction->addLocal("$iter");
    fb.storeLocal(iterSlot);

    // Store index
    uint8_t idxSlot = currentFunction->addLocal("$idx");
    fb.pushConst(builder.addConstInt(0));
    fb.storeLocal(idxSlot);

    // Add loop variable
    uint8_t loopVarSlot = currentFunction->addLocal(stmt->variable);

    LoopContext loopCtx;
    loopCtx.loopStart = fb.currentPos();
    loopStack.push_back(loopCtx);

    // Check if index < length (simplified - would need proper array length check)
    // For now, just iterate and break on nil
    fb.loadLocal(iterSlot);
    fb.loadLocal(idxSlot);
    fb.emit(OP_ARRAY_GET);
    fb.emit(OP_DUP);

    // Store in loop variable
    fb.storeLocal(loopVarSlot);

    // Check if nil (end of iteration)
    fb.pushConst(builder.addConstNil());
    fb.emit(OP_EQ);

    uint32_t exitJump = fb.currentPos();
    fb.jumpIfTrue(0);  // Placeholder

    // Generate body
    generateStmt(stmt->body.get(), fb);

    // Increment index
    fb.loadLocal(idxSlot);
    fb.pushConst(builder.addConstInt(1));
    fb.emit(OP_ADD);
    fb.storeLocal(idxSlot);

    // Jump back to start
    fb.jump(loopCtx.loopStart);

    // Patch exit
    uint32_t endPos = fb.currentPos();
    patchJump(fb, exitJump + 1, endPos);

    // Patch break/continue
    for (uint32_t breakPos : loopStack.back().breakJumps) {
        patchJump(fb, breakPos, endPos);
    }
    for (uint32_t continuePos : loopStack.back().continueJumps) {
        patchJump(fb, continuePos, loopCtx.loopStart);
    }

    loopStack.pop_back();
    currentFunction->exitScope();
}

void CodeGenerator::generateLoop(LoopStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    LoopContext loopCtx;
    loopCtx.loopStart = fb.currentPos();
    loopStack.push_back(loopCtx);

    // Generate body
    generateStmt(stmt->body.get(), fb);

    // Jump back to start
    fb.jump(loopCtx.loopStart);

    // Note: infinite loop - break statement needed to exit
    uint32_t endPos = fb.currentPos();

    // Patch break jumps
    for (uint32_t breakPos : loopStack.back().breakJumps) {
        patchJump(fb, breakPos, endPos);
    }

    // Patch continue jumps
    for (uint32_t continuePos : loopStack.back().continueJumps) {
        patchJump(fb, continuePos, loopCtx.loopStart);
    }

    loopStack.pop_back();
}

void CodeGenerator::generateReturn(ReturnStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    if (stmt->value) {
        generateExpr(stmt->value.get(), fb);

        // Record location before return
        if (generateDebugInfo && stmt->line > 0) {
            recordDebugLocation(fb, stmt->line, stmt->column);
        }

        fb.ret(1);
    } else {
        if (generateDebugInfo && stmt->line > 0) {
            recordDebugLocation(fb, stmt->line, stmt->column);
        }
        fb.ret(0);
    }
}

void CodeGenerator::generateBreak(DBCBuilder::FunctionBuilder& fb) {
    if (loopStack.empty()) {
        return;
    }

    uint32_t jumpPos = fb.currentPos();
    fb.jump(0);  // Placeholder
    loopStack.back().breakJumps.push_back(jumpPos + 1);
}

void CodeGenerator::generateContinue(DBCBuilder::FunctionBuilder& fb) {
    if (loopStack.empty()) {
        return;
    }

    uint32_t jumpPos = fb.currentPos();
    fb.jump(0);  // Placeholder
    loopStack.back().continueJumps.push_back(jumpPos + 1);
}

void CodeGenerator::generateExprStmt(ExprStmt* stmt, DBCBuilder::FunctionBuilder& fb) {
    generateExpr(stmt->expr.get(), fb);
    fb.emit(OP_POP);  // Pop unused result
}

// ============================================================================
// Expression Code Generation
// ============================================================================

void CodeGenerator::generateExpr(Expr* expr, DBCBuilder::FunctionBuilder& fb) {
    if (generateDebugInfo && expr->line > 0) {
        recordDebugLocation(fb, expr->line, expr->column);
    }

    if (auto lit = dynamic_cast<LiteralExpr*>(expr)) {
        generateLiteral(lit, fb);
    } else if (auto id = dynamic_cast<IdentifierExpr*>(expr)) {
        generateIdentifier(id, fb);
    } else if (auto bin = dynamic_cast<BinaryExpr*>(expr)) {
        generateBinary(bin, fb);
    } else if (auto un = dynamic_cast<UnaryExpr*>(expr)) {
        generateUnary(un, fb);
    } else if (auto assign = dynamic_cast<AssignExpr*>(expr)) {
        generateAssign(assign, fb);
    } else if (auto compAssign = dynamic_cast<CompoundAssignExpr*>(expr)) {
        generateCompoundAssign(compAssign, fb);
    } else if (auto call = dynamic_cast<CallExpr*>(expr)) {
        generateCall(call, fb);
    } else if (auto field = dynamic_cast<FieldAccessExpr*>(expr)) {
        generateFieldAccess(field, fb);
    } else if (auto index = dynamic_cast<IndexExpr*>(expr)) {
        generateIndex(index, fb);
    } else if (auto newExpr = dynamic_cast<NewExpr*>(expr)) {
        generateNew(newExpr, fb);
    } else if (auto list = dynamic_cast<ListExpr*>(expr)) {
        generateList(list, fb);
    } else if (auto dict = dynamic_cast<DictExpr*>(expr)) {
        generateDict(dict, fb);
    } else if (auto cast = dynamic_cast<CastExpr*>(expr)) {
        generateCast(cast, fb);
    } else if (auto isExpr = dynamic_cast<IsExpr*>(expr)) {
        generateIs(isExpr, fb);
    }
}

void CodeGenerator::generateLiteral(LiteralExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    switch (expr->type) {
        case LiteralExpr::Type::INT: {
            int64_t val = std::get<int64_t>(expr->value);
            fb.pushConst(builder.addConstInt(static_cast<int32_t>(val)));
            break;
        }
        case LiteralExpr::Type::FLOAT: {
            double val = std::get<double>(expr->value);
            fb.pushConst(builder.addConstDouble(val));
            break;
        }
        case LiteralExpr::Type::BOOL: {
            bool val = std::get<bool>(expr->value);
            fb.pushConst(builder.addConstBool(val));
            break;
        }
        case LiteralExpr::Type::STRING: {
            std::string val = std::get<std::string>(expr->value);
            fb.pushConst(addStringConstant(val));
            break;
        }
        case LiteralExpr::Type::NULLVAL: {
            fb.pushConst(builder.addConstNil());
            break;
        }
    }
}

void CodeGenerator::generateIdentifier(IdentifierExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Check if it's a local variable
    int localSlot = currentFunction->findLocal(expr->name);
    if (localSlot >= 0) {
        fb.loadLocal(static_cast<uint8_t>(localSlot));
        return;
    }

    // Check if it's a global or static field
    uint32_t globalIdx = getOrAddGlobal(expr->name);
    fb.loadGlobal(globalIdx);
}

void CodeGenerator::generateUnary(UnaryExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    generateExpr(expr->operand.get(), fb);

    switch (expr->op) {
        case UnaryExpr::Op::NEG:
            // Negate: 0 - value
            fb.pushConst(builder.addConstInt(0));
            fb.emit(OP_SWAP);
            fb.emit(OP_SUB);
            break;
        case UnaryExpr::Op::NOT:
            fb.emit(OP_NOT);
            break;
    }
}

void CodeGenerator::generateAssign(AssignExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Handle different assignment targets
    if (auto id = dynamic_cast<IdentifierExpr*>(expr->target.get())) {
        // Simple variable assignment
        generateExpr(expr->value.get(), fb);

        int localSlot = currentFunction->findLocal(id->name);
        if (localSlot >= 0) {
            fb.emit(OP_DUP);  // Keep value on stack
            fb.storeLocal(static_cast<uint8_t>(localSlot));
        } else {
            fb.emit(OP_DUP);
            uint32_t globalIdx = getOrAddGlobal(id->name);
            fb.storeGlobal(globalIdx);
        }
    } else if (auto field = dynamic_cast<FieldAccessExpr*>(expr->target.get())) {
        // Field assignment: obj.field = value
        generateExpr(field->object.get(), fb);
        generateExpr(expr->value.get(), fb);
        fb.emit(OP_DUP);  // Duplicate value
        fb.emit(OP_ROT);  // Bring object to top
        fb.emit(OP_SWAP); // value obj value -> obj value value
        uint32_t fieldIdx = addStringConstant(field->field);
        fb.setField(fieldIdx);
    } else if (auto index = dynamic_cast<IndexExpr*>(expr->target.get())) {
        // Index assignment: arr[idx] = value
        generateExpr(index->object.get(), fb);
        generateExpr(index->index.get(), fb);
        generateExpr(expr->value.get(), fb);
        fb.emit(OP_DUP);  // Keep value on stack
        fb.emit(OP_ARRAY_SET);
    }
}

void CodeGenerator::generateCompoundAssign(CompoundAssignExpr* expr,
                                            DBCBuilder::FunctionBuilder& fb) {
    // Load current value
    if (auto id = dynamic_cast<IdentifierExpr*>(expr->target.get())) {
        int localSlot = currentFunction->findLocal(id->name);
        if (localSlot >= 0) {
            fb.loadLocal(static_cast<uint8_t>(localSlot));
        } else {
            uint32_t globalIdx = getOrAddGlobal(id->name);
            fb.loadGlobal(globalIdx);
        }

        // Generate value
        generateExpr(expr->value.get(), fb);

        // Perform operation
        switch (expr->op) {
            case CompoundAssignExpr::Op::ADD:
                fb.emit(OP_ADD);
                break;
            case CompoundAssignExpr::Op::SUB:
                fb.emit(OP_SUB);
                break;
        }

        // Store result
        if (localSlot >= 0) {
            fb.emit(OP_DUP);
            fb.storeLocal(static_cast<uint8_t>(localSlot));
        } else {
            fb.emit(OP_DUP);
            uint32_t globalIdx = getOrAddGlobal(id->name);
            fb.storeGlobal(globalIdx);
        }
    }
}

void CodeGenerator::generateCall(CallExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Check if it's a method call
    if (auto fieldAccess = dynamic_cast<FieldAccessExpr*>(expr->callee.get())) {

        // CHECK FOR STATIC METHOD CALL (ClassName.methodName)
        if (auto classId = dynamic_cast<IdentifierExpr*>(fieldAccess->object.get())) {
            // Check if the identifier is a class name (not a variable)
            auto classIt = typeChecker.getClassInfo().find(classId->name);
            if (classIt != typeChecker.getClassInfo().end()) {
                // This is a static method call
                std::string mangledName = mangleName(classId->name, fieldAccess->field);
                auto it = functionIndices.find(mangledName);

                if (it != functionIndices.end()) {
                    std::cerr << "DEBUG: Calling static method '" << mangledName
                              << "' at index " << it->second << "\n";

                    // For static methods, NO self parameter!
                    // Just push the arguments
                    for (auto& arg : expr->arguments) {
                        generateExpr(arg.get(), fb);
                    }

                    uint8_t argc = static_cast<uint8_t>(expr->arguments.size());
                    fb.call(it->second, argc);
                    return;
                } else {
                    std::cerr << "Error: Static method '" << mangledName << "' not found\n";
                    fb.pushConst(builder.addConstNil());
                    return;
                }
            }
        }

        // Regular instance method call
        // Get the type from type checking
        std::string className;

        if (fieldAccess->object->type &&
            fieldAccess->object->type->kind == Type::Kind::OBJECT) {
            className = fieldAccess->object->type->className;
        }

        if (!className.empty()) {
            // Check for method in this class and parent classes
            std::string mangledName = findMethodInClass(className, fieldAccess->field);

            if (!mangledName.empty()) {
                auto it = functionIndices.find(mangledName);
                if (it != functionIndices.end()) {
                    // Push self FIRST
                    generateExpr(fieldAccess->object.get(), fb);

                    // Push arguments AFTER self
                    for (auto& arg : expr->arguments) {
                        generateExpr(arg.get(), fb);
                    }

                    // argc includes self + all arguments
                    uint8_t argc = static_cast<uint8_t>(expr->arguments.size() + 1);
                    if (generateDebugInfo && expr->line > 0) {
                        recordDebugLocation(fb, expr->line, expr->column);
                    }

                    fb.call(it->second, argc);
                    return;
                } else {
                    std::cerr << "Error: Function index not found for '" << mangledName << "'\n";
                }
            } else {
                std::cerr << "Error: Could not find method '" << fieldAccess->field
                          << "' in class '" << className << "'\n";
            }
        }

        std::cerr << "Warning: Could not resolve method '" << fieldAccess->field << "'\n";
        fb.pushConst(builder.addConstNil());
        return;
    }

    else if (auto id = dynamic_cast<IdentifierExpr*>(expr->callee.get())) {
        // Function call or constructor call

        // Check if it's an FFI call FIRST
        const auto ffiIt = ffiRegistry.find(id->name);
        if (ffiIt != ffiRegistry.end()) {
            const FFIInfo* ffiDecl = ffiIt->second;

            // Push arguments onto stack
            for (auto& arg : expr->arguments) {
                generateExpr(arg.get(), fb);
            }

            const auto argc = static_cast<uint8_t>(expr->arguments.size());

            // Add library path, symbol name, and signature to constants
            const uint32_t libIdx = addStringConstant(ffiDecl->libPath);
            const uint32_t symIdx = addStringConstant(id->name);
            const uint32_t sigIdx = addStringConstant(ffiDecl->sig);

            if (generateDebugInfo && expr->line > 0) {
                recordDebugLocation(fb, expr->line, expr->column);
            }

            fb.callFFI(libIdx, symIdx, argc, sigIdx);
            return;
        }



        // Check for built-in functions
        if (isBuiltinFunction(id->name)) {
            // Push arguments
            for (auto& arg : expr->arguments) {
                generateExpr(arg.get(), fb);
            }

            std::string builtinName = getBuiltinFunctionName(id->name);
            uint32_t nameIdx = addStringConstant(builtinName);
            uint8_t argc = static_cast<uint8_t>(expr->arguments.size());

            if (generateDebugInfo && expr->line > 0) {
                recordDebugLocation(fb, expr->line, expr->column);
            }

            fb.emit(OP_CALL_NATIVE);
            fb.emitU32(nameIdx);
            fb.emitU8(argc);
            return;
        }

        // Check if it's a constructor call (class name used as function)
        std::string ctorName = mangleName(id->name, "new");
        auto ctorIt = functionIndices.find(ctorName);

        if (ctorIt != functionIndices.end()) {
            std::cerr << "DEBUG: Calling constructor '" << ctorName
                      << "' at index " << ctorIt->second << "\n";

            // This is a constructor call
            for (auto& arg : expr->arguments) {
                generateExpr(arg.get(), fb);
            }

            uint8_t argc = static_cast<uint8_t>(expr->arguments.size());
            fb.call(ctorIt->second, argc);
            return;
        }

        // Regular function call
        auto it = functionIndices.find(id->name);
        if (it != functionIndices.end()) {
            std::cerr << "DEBUG: Calling function '" << id->name
                      << "' at index " << it->second << "\n";

            for (auto& arg : expr->arguments) {
                generateExpr(arg.get(), fb);
            }

            uint8_t argc = static_cast<uint8_t>(expr->arguments.size());

            fb.call(it->second, argc);
            return;
        }

        std::cerr << "Error: Undefined function '" << id->name << "'\n";
        fb.pushConst(builder.addConstNil());
    }
}

void CodeGenerator::generateMethod(FunctionDecl* method, const std::string& className) {
    std::string methodName = mangleName(className, method->name);

    currentFunctionIndex = static_cast<uint32_t>(builder.functions.size());

    // Initialize debug info
    if (generateDebugInfo) {
        FunctionDebugInfo& info = debugInfoMap[currentFunctionIndex];
        info.name = methodName;
        info.file = currentSourceFile;
    }

    functionIndices[methodName] = currentFunctionIndex;

    auto& fb = builder.addFunction(methodName);

    uint8_t paramCount;
    if (method->isStatic) {
        paramCount = static_cast<uint8_t>(method->params.size());
    } else {
        paramCount = static_cast<uint8_t>(method->params.size() + 1);
    }
    fb.setArgCount(paramCount);

    FunctionContext ctx;
    ctx.builder = &fb;
    ctx.className = className;
    currentFunction = &ctx;

    if (!method->isStatic) {
        ctx.addLocal("self");
    }

    for (size_t i = 0; i < method->params.size(); i++) {
        ctx.addLocal(method->params[i].name);
    }

    ctx.localCount = static_cast<uint8_t>(ctx.locals.size());

    // Generate method body
    if (method->body) {
        auto* blockStmt = dynamic_cast<BlockStmt*>(method->body.get());
        if (blockStmt) {
            for (auto& stmt : blockStmt->statements) {
                generateStmt(stmt.get(), fb);
            }
        } else {
            generateStmt(method->body.get(), fb);
        }
    }

    if (method->returnType.empty() || method->returnType == "void") {
        fb.pushConst(builder.addConstNil());
        fb.ret(1);
    } else {
        fb.pushConst(builder.addConstNil());
        fb.ret(1);
    }

    fb.setLocalCount(ctx.localCount);

    // Finalize debug info
    if (generateDebugInfo) {
        finalizeDebugInfo(currentFunctionIndex, methodName);
    }

    currentFunction = nullptr;
}

// Also fix regular functions:
void CodeGenerator::generateFunctionBody(FunctionDecl* func, DBCBuilder::FunctionBuilder& fb) {
    // Add parameters to locals
    for (const auto& param : func->params) {
        currentFunction->addLocal(param.name);
    }

    currentFunction->localCount = static_cast<uint8_t>(currentFunction->locals.size());

    // Generate function body
    if (func->body) {
        auto* blockStmt = dynamic_cast<BlockStmt*>(func->body.get());
        if (blockStmt) {
            for (auto& stmt : blockStmt->statements) {
                generateStmt(stmt.get(), fb);
            }
        } else {
            generateStmt(func->body.get(), fb);
        }
    }

    // CRITICAL FIX: ALL functions must return something!
    // Even void functions need to push nil
    if (func->returnType.empty() || func->returnType == "void") {
        fb.pushConst(builder.addConstNil());  // Push nil for void functions
        fb.ret(1);  // Return 1 value (the nil)
    }
    else {
        // If no explicit return, return nil
        fb.pushConst(builder.addConstNil());
        fb.ret(1);
    }

    fb.setLocalCount(currentFunction->localCount);
}

void CodeGenerator::debugPrintIndices() {
    std::cerr << "\n=== Function Indices ===\n";
    for (const auto& [name, idx] : functionIndices) {
        std::cerr << "  [" << idx << "] " << name << "\n";
    }
    std::cerr << "========================\n\n";
}

void CodeGenerator::generateFieldAccess(FieldAccessExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    generateExpr(expr->object.get(), fb);
    uint32_t fieldIdx = addStringConstant(expr->field);
    fb.getField(fieldIdx);
}

void CodeGenerator::generateIndex(IndexExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    generateExpr(expr->object.get(), fb);
    generateExpr(expr->index.get(), fb);
    fb.arrayGet();
}

void CodeGenerator::generateNew(NewExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Call constructor
    std::string ctorName = mangleName(expr->className, "new");

    // Push arguments
    for (auto& arg : expr->arguments) {
        generateExpr(arg.get(), fb);
    }

    // Call constructor
    auto it = functionIndices.find(ctorName);
    uint32_t ctorIdx;
    if (it != functionIndices.end()) {
        ctorIdx = it->second;
    } else {
        ctorIdx = addStringConstant(ctorName);
    }

    uint8_t argc = static_cast<uint8_t>(expr->arguments.size());
    fb.call(ctorIdx, argc);
}

void CodeGenerator::generateList(ListExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Create new array
    fb.newArray();

    // Add elements
    for (size_t i = 0; i < expr->elements.size(); i++) {
        fb.emit(OP_DUP);  // Duplicate array reference
        fb.pushConst(builder.addConstInt(static_cast<int32_t>(i)));
        generateExpr(expr->elements[i].get(), fb);
        fb.arraySet();
    }
}

void CodeGenerator::generateDict(DictExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Create new map
    fb.newMap();

    // Add key-value pairs
    for (auto& pair : expr->pairs) {
        fb.emit(OP_DUP);  // Duplicate map reference
        generateExpr(pair.first.get(), fb);   // Key
        generateExpr(pair.second.get(), fb);  // Value
        fb.emit(OP_MAP_SET);
    }
}

void CodeGenerator::generateCast(CastExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Generate expression
    generateExpr(expr->expr.get(), fb);

    // For now, casting is mostly a no-op at runtime
    // Type checking was done during semantic analysis
    // We might add runtime type checks for safety

    // TODO: Add runtime type validation if needed
}

void CodeGenerator::generateIs(IsExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Generate the expression to check
    generateExpr(expr->expr.get(), fb);

    // Push the target type name
    uint32_t targetTypeIdx = addStringConstant(expr->targetType);

    // Emit IS_INSTANCE with the type name index
    fb.emit(OP_IS_INSTANCE);
    fb.emitU32(targetTypeIdx);
}

// ============================================================================
// Helper Methods
// ============================================================================

void CodeGenerator::patchJump(DBCBuilder::FunctionBuilder& fb,
                               uint32_t jumpPos, uint32_t target) {
    // Patch a jump instruction's target
    // The jumpPos points to the first byte of the 4-byte target address
    fb.code[jumpPos] = static_cast<uint8_t>(target & 0xFF);
    fb.code[jumpPos + 1] = static_cast<uint8_t>((target >> 8) & 0xFF);
    fb.code[jumpPos + 2] = static_cast<uint8_t>((target >> 16) & 0xFF);
    fb.code[jumpPos + 3] = static_cast<uint8_t>((target >> 24) & 0xFF);
}

Op CodeGenerator::getBinaryOp(BinaryExpr::Op op) {
    switch (op) {
        case BinaryExpr::Op::ADD: return OP_ADD;
        case BinaryExpr::Op::SUB: return OP_SUB;
        case BinaryExpr::Op::MUL: return OP_MUL;
        case BinaryExpr::Op::DIV: return OP_DIV;
        case BinaryExpr::Op::MOD: return OP_MOD;
        case BinaryExpr::Op::EQ:  return OP_EQ;
        case BinaryExpr::Op::NEQ: return OP_NEQ;
        case BinaryExpr::Op::LT:  return OP_LT;
        case BinaryExpr::Op::LTE: return OP_LTE;
        case BinaryExpr::Op::GT:  return OP_GT;
        case BinaryExpr::Op::GTE: return OP_GTE;
        case BinaryExpr::Op::AND: return OP_AND;
        case BinaryExpr::Op::OR:  return OP_OR;
    }
    return OP_ADD; // Default
}

bool CodeGenerator::isBuiltinFunction(const std::string& name) {
    // List of built-in functions that should use CALL_NATIVE
    static const std::vector<std::string> builtins = {
        "println", "print", "input", "str", "int", "float", "exit",
        "len", "push", "pop", "get", "set", "has", "del",
        "keys", "values", "substr", "charAt", "concat", "android_native_toast"
    };

    for (const auto& builtin : builtins) {
        if (builtin == name) {
            return true;
        }
    }
    return false;
}

std::string CodeGenerator::getBuiltinFunctionName(const std::string& name) {
    // Return the native function name registered in VM
    // For most cases, it's the same as the Droplet name
    return name;
}

std::string CodeGenerator::findMethodInClass(const std::string& className,
                                              const std::string& methodName) {
    // Check current class
    const auto& classInfo = typeChecker.getClassInfo();
    auto it = classInfo.find(className);

    if (it != classInfo.end()) {
        // Check if method exists in this class
        if (it->second.methods.find(methodName) != it->second.methods.end()) {
            return mangleName(className, methodName);
        }

        // Check parent class recursively
        if (!it->second.parentClass.empty()) {
            return findMethodInClass(it->second.parentClass, methodName);
        }
    }

    return "";
}

void CodeGenerator::generateBinary(BinaryExpr* expr, DBCBuilder::FunctionBuilder& fb) {
    // Check if this binary expression has an operator overload
    if (expr->hasOperatorOverload && !expr->operatorMethodName.empty()) {
        // This is an operator overload - call the method
        std::string className = expr->left->type->className;
        std::string mangledName = mangleName(className, expr->operatorMethodName);

        auto it = functionIndices.find(mangledName);
        if (it != functionIndices.end()) {
            std::cerr << "DEBUG: Calling operator overload '" << mangledName
                      << "' at index " << it->second << "\n";

            // Record location before the call
            if (generateDebugInfo && expr->line > 0) {
                recordDebugLocation(fb, expr->line, expr->column);
            }

            // Push self (left operand)
            generateExpr(expr->left.get(), fb);
            // Push argument (right operand)
            generateExpr(expr->right.get(), fb);

            // Call the operator method (self + 1 arg = 2 args total)
            fb.call(it->second, 2);
            return;
        } else {
            std::cerr << "ERROR: Operator method '" << mangledName
                      << "' not found in function indices\n";
        }
    }

    // Generate operands for regular operations
    generateExpr(expr->left.get(), fb);
    generateExpr(expr->right.get(), fb);

    // Record location before emitting the operator
    if (generateDebugInfo && expr->line > 0) {
        recordDebugLocation(fb, expr->line, expr->column);
    }

    // Special handling for string concatenation
    if (expr->op == BinaryExpr::Op::ADD) {
        // Check if both operands are strings
        if (expr->left->type && expr->right->type &&
            expr->left->type->kind == Type::Kind::STRING &&
            expr->right->type->kind == Type::Kind::STRING) {
            fb.emit(OP_STRING_CONCAT);
            return;
        }
    }

    // Default numeric operation (fallback)
    Op op = getBinaryOp(expr->op);
    fb.emit(op);
}

void CodeGenerator::recordDebugLocation(DBCBuilder::FunctionBuilder& fb, uint32_t line, uint32_t column) {
    if (!generateDebugInfo || currentSourceFile.empty()) {
        return;
    }

    // Avoid recording duplicate locations for sequential instructions
    if (line == lastRecordedLine && column == lastRecordedColumn) {
        return;
    }

    uint32_t ip = fb.currentPos();

    // Get or create debug info for current function
    FunctionDebugInfo& info = debugInfoMap[currentFunctionIndex];

    // Record IP -> SourceLocation mapping
    info.ipToLocation[ip] = SourceLocation{currentSourceFile, line, column};

    lastRecordedLine = line;
    lastRecordedColumn = column;
}


void CodeGenerator::finalizeDebugInfo(uint32_t funcIdx, const std::string& funcName) {
    if (!generateDebugInfo) {
        return;
    }

    FunctionDebugInfo& info = debugInfoMap[funcIdx];

    if (info.name.empty()) {
        info.name = funcName;
    }
    if (info.file.empty()) {
        info.file = currentSourceFile;
    }

    // Copy local variables from FunctionContext
    if (currentFunction) {
        for (const auto& [varName, slot, s] : currentFunction->locals) {
            info.localVariables[varName] = slot;
        }
    }
}
