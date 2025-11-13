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
 *  File: Program
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_PROGRAM_H
#define DROPLET_PROGRAM_H
#include <memory>
#include <vector>

struct FFIDecl;
struct FunctionDecl;
struct ClassDecl;
struct ImportStmt;
struct ModuleDecl;

struct Program {
    std::unique_ptr<ModuleDecl> moduleDecl;  // nullptr if no module declaration
    std::vector<std::unique_ptr<ImportStmt>> imports;
    std::vector<std::unique_ptr<ClassDecl>> classes;
    std::vector<std::unique_ptr<FunctionDecl>> functions;
    std::vector<std::unique_ptr<FFIDecl>> ffiDecls;
};

#endif //DROPLET_PROGRAM_H