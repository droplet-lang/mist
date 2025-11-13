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
 *  File: FFI
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_FFI_H
#define DROPLET_FFI_H
#include <string>
#include <unordered_map>
#include <vector>

#include "Value.h"

typedef int ffi_type;

struct VM;

struct FFISignature {
    std::vector<char> argTypes;
    char returnType;
};

struct FFIHelper {
    std::unordered_map<std::string, void*> libs;

//    void* load_lib(const std::string &path);
//
//    static void* find_symbol(void* lib, const std::string &symbol);
//
//    static ffi_type *get_ffi_type(char t);

    static Value do_ffi_call(const std::string &sig, std::vector<Value> &args, VM *vm, void* fn_ptr);
};

#endif //DROPLET_FFI_H