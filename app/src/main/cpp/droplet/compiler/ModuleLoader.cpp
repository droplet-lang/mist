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
 *  File: ModuleLoader
 *  Created: 11/9/2025
 * ============================================================
 */
#include "ModuleLoader.h"
#include <iostream>
#include <fstream>

#include "Lexer.h"
#include "Parser.h"
#include "TypeChecker.h"

ModuleLoader::ModuleLoader() {
    // Add default search paths
    searchPaths.emplace_back("."); // Current directory
    searchPaths.emplace_back("./.dp_modules"); //  similar like node_modules

    // We can make the libraries shared as well??
    // or should we make this as like dart which just support including from inside the folder??
    // TODO: (@svpz) later we can add a PATH variable where shared things can be downloaded
    // similar like global download and project specific download
}

void ModuleLoader::addSearchPath(const std::string &path) {
    searchPaths.push_back(path);
}

std::string ModuleLoader::modulePathToFilePath(const std::string &modulePath) {
    std::string filePath = modulePath;

    for (char &c: filePath) {
        if (c == '.') {
            c = '/';
        }
    }

    filePath += ".drop";
    return filePath;
}

std::string ModuleLoader::resolveModulePath(const std::string& modulePath) const {
    const std::string relativeFile = modulePathToFilePath(modulePath);
    const fs::path relativePath(relativeFile);

    for (const auto& searchPath : searchPaths) {

        if (!fs::exists(searchPath)) {
            std::cerr << "  [!] Path does not exist, skipping.\n";
            continue;
        }

        try {
            for (auto& entry : fs::recursive_directory_iterator(
                     searchPath, fs::directory_options::skip_permission_denied)) {

                if (!entry.is_regular_file())
                    continue;

                const fs::path& entryPath = entry.path();

                if (fs::path rel = fs::relative(entryPath, searchPath); rel == relativePath) {
                    return entryPath.string();
                }
                     }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "  [Warning] Could not access directory '" << searchPath << "': " << e.what() << "\n";
            continue;
        }
    }

    return "";  // Not found
}

std::unique_ptr<Program> ModuleLoader::parseModuleFile(const std::string &filePath) {
    // Read file
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open module file: " << filePath << "\n";
        return nullptr;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();

    // Lex
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    // Parse
    Parser parser(tokens);
    try {
        Program program = parser.parse();
        return std::make_unique<Program>(std::move(program));
    } catch (const ParseError &e) {
        std::cerr << "Error parsing module " << filePath << ": " << e.what() << "\n";
        return nullptr;
    }
}

void ModuleLoader::extractExports(ModuleInfo *module) {
    if (!module || !module->ast) return;

    // Extract all top-level functions
    for (const auto &func: module->ast->functions) {
        module->exportedFunctions.push_back(func->name);
    }

    // Extract all classes
    for (const auto &cls: module->ast->classes) {
        module->exportedClasses.push_back(cls->name);
    }

    std::cerr << "Module '" << module->modulePath << "' exports:\n";
    std::cerr << "  Functions: ";
    for (const auto &name: module->exportedFunctions) {
        std::cerr << name << " ";
    }
    std::cerr << "\n  Classes: ";
    for (const auto &name: module->exportedClasses) {
        std::cerr << name << " ";
    }
    std::cerr << "\n";
}

ModuleInfo *ModuleLoader::loadModule(const std::string &modulePath) {
    // Check if already loaded
    const auto it = loadedModules.find(modulePath);
    if (it != loadedModules.end()) {
        std::cerr << "Module '" << modulePath << "' already loaded\n";
        return it->second.get();
    }

    std::cerr << "Loading module: " << modulePath << "\n";

    // Resolve file path
    const std::string filePath = resolveModulePath(modulePath);
    if (filePath.empty()) {
        std::cerr << "Error: Module '" << modulePath << "' not found in search paths:\n";
        for (const auto &path: searchPaths) {
            std::cerr << "  - " << path << "\n";
        }
        return nullptr;
    }

    std::cerr << "Found module at: " << filePath << "\n";

    // Parse module
    auto ast = parseModuleFile(filePath);
    if (!ast) {
        return nullptr;
    }

    // Create module info
    auto module = std::make_unique<ModuleInfo>();
    module->modulePath = modulePath;
    module->filePath = filePath;
    module->ast = std::move(ast);
    module->isCompiled = false;
    module->isTypeChecked = false;
    module->moduleTypeChecker = nullptr;


    // Generate DBC path: "std.collections" -> "std/collections.dbc"
    std::string dbcRelPath = modulePathToFilePath(modulePath);
    dbcRelPath = dbcRelPath.substr(0, dbcRelPath.length() - 5) + ".dbc";
    module->dbcPath = dbcRelPath;

    // Extract exports
    extractExports(module.get());

    // Store module
    ModuleInfo *modulePtr = module.get();
    loadedModules[modulePath] = std::move(module);

    // Recursively load imports
    for (const auto &import: modulePtr->ast->imports) {
        loadModule(import->modulePath);
    }

    return modulePtr;
}

bool ModuleLoader::isLoaded(const std::string &modulePath) const {
    return loadedModules.contains(modulePath);
}

ModuleInfo *ModuleLoader::getModule(const std::string &modulePath) {
    const auto it = loadedModules.find(modulePath);
    if (it != loadedModules.end()) {
        return it->second.get();
    }
    return nullptr;
}
