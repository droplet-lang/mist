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
 *  File: Native
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_NATIVE_H
#define DROPLET_NATIVE_H
#include "../vm/VM.h"

void native_len(VM& vm, uint8_t argc);
void native_print(VM& vm, uint8_t argc);
void native_println(VM& vm, uint8_t argc);
void native_str(VM& vm, uint8_t argc);
void native_input(VM& vm, uint8_t argc);
void native_int(VM& vm, uint8_t argc);
void native_float(VM& vm, uint8_t argc);
void native_exit(VM& vm, uint8_t argc);

// android
void android_native_toast(VM& vm, const uint8_t argc);

// made inline just to shut up compiler warning, no special case
inline void register_native_functions(VM& vm) {
    vm.register_native("exit", native_exit);
    vm.register_native("print", native_print);
    vm.register_native("println2", native_println);
    vm.register_native("str", native_str);
    vm.register_native("len", native_len);
    vm.register_native("input", native_input);
    vm.register_native("float", native_float);
    vm.register_native("int", native_int);

    // android
    vm.register_native("println", android_native_toast);

}

#endif //DROPLET_NATIVE_H