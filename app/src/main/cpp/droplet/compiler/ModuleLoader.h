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
#ifndef DROPLET_MODULELOADER_H
#define DROPLET_MODULELOADER_H
#include <map>
#include <memory>
#include <string>
#include <filesystem>

#include "Program.h"

class TypeChecker;
namespace fs = std::filesystem;

struct ModuleInfo {
    std::string modulePath;
    std::string filePath;
    std::unique_ptr<Program> ast;
    bool isCompiled;
    bool isTypeChecked;
    std::string dbcPath;

    // Exported symbols
    std::vector<std::string> exportedFunctions;
    std::vector<std::string> exportedClasses;

    std::unique_ptr<TypeChecker> moduleTypeChecker;
};

class ModuleLoader {
public:
    ModuleLoader();

    // Add search path for modules
    void addSearchPath(const std::string& path);

    // Load a module by path 
    ModuleInfo* loadModule(const std::string& modulePath);

    // Check if module is already loaded
    bool isLoaded(const std::string& modulePath) const;

    // Get loaded module
    ModuleInfo* getModule(const std::string& modulePath);

    // Get all loaded modules
    const std::map<std::string, std::unique_ptr<ModuleInfo>>& getLoadedModules() const {
        return loadedModules;
    }

    // Resolve module path to file path
    std::string resolveModulePath(const std::string& modulePath) const;

private:
    std::vector<std::string> searchPaths;
    std::map<std::string, std::unique_ptr<ModuleInfo>> loadedModules;

    // Convert "std.collections" to "std/collections.drop"
    static std::string modulePathToFilePath(const std::string& modulePath);

    // Parse a module file
    static std::unique_ptr<Program> parseModuleFile(const std::string& filePath);

    // Extract exported symbols from module
    static void extractExports(ModuleInfo* module);
};


#endif //DROPLET_MODULELOADER_H