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
 *  File: VM
 *  Created: 11/7/2025
 * ============================================================
 */
#ifndef DROPLET_VM_H
#define DROPLET_VM_H
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Allocator.h"
#include "CallFrame.h"
#include "defines.h"
#include "FFIHelper.h"
#include "StackManager.h"
#include "Value.h"

class Debugger;

struct VM {
    VM() {
        stack_manager.sp = 0;
    }

    StackManager stack_manager;
    FFIHelper ffi;
    Allocator allocator;
    Debugger* debugger = nullptr;
    bool debugMode = false;

    void setDebugger(Debugger* dbg) {
        debugger = dbg;
        debugMode = (dbg != nullptr);
    }

    // Native registry
    std::unordered_map<std::string, NativeFunction> native_functions_registry;
    void register_native(const std::string &name, NativeFunction fn);

    // global table
    std::unordered_map<std::string, Value> globals;
    std::vector<Value> global_constants;
    uint32_t add_global_string_constant(const std::string &s);

    // frames
    std::vector<CallFrame> call_frames;

    // "RETURN 2" means return top 2 value from stack
    // This is so that we can have go like err handling
    void do_return(uint8_t return_count);


    // functions (loaded module)
    std::vector<std::unique_ptr<Function>> functions;
    std::unordered_map<std::string, uint32_t> function_index_by_name;

    uint32_t get_function_index(const std::string &name);
    void call_function_by_index(uint32_t fnIndex, size_t argCount);


    // This is going to be huge rn, won-t refactor for clarity unless finalized
    void run();
};


#endif //DROPLET_VM_H