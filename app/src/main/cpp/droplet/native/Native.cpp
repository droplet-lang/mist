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
#include "Native.h"

#include <iostream>
#include "../vm/VM.h"

#if defined(__ANDROID__)
#include <android/log.h>
#define LOG_TAG "DropletVM"
#endif

void native_print(VM& vm, const uint8_t argc) {
    std::string output;

    // Collect output
    for (int i = argc - 1; i >= 0; i--) {
        Value v = vm.stack_manager.peek(i);
        output += v.toString();
        if (i > 0) output += " ";
    }

#if defined(__ANDROID__)
    // Android: print to Logcat
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", output.c_str());
#else
    // Desktop/CLI: print to stdout
    std::cout << output;
    std::cout.flush();
#endif

    // Clean up stack
    for (int i = 0; i < argc; i++) vm.stack_manager.pop();
    vm.stack_manager.push(Value::createNIL());
}

// Native println function
void native_println(VM& vm, const uint8_t argc) {
    std::string output;
    for (int i = argc - 1; i >= 0; i--) {
        Value v = vm.stack_manager.peek(i);
        output += v.toString();
        if (i > 0) output += " ";
    }

#if defined(__ANDROID__)
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", output.c_str());
#else
    std::cout << output << std::endl;
#endif

    for (int i = 0; i < argc; i++) vm.stack_manager.pop();
    vm.stack_manager.push(Value::createNIL());
}


// Native str function
void native_str(VM& vm, const uint8_t argc) {
    if (argc != 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }
    Value v = vm.stack_manager.pop();
    ObjString* str = vm.allocator.allocate_string(v.toString());
    vm.stack_manager.push(Value::createOBJECT(str));
}

// Native len function
void native_len(VM& vm, const uint8_t argc) {
    if (argc != 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createINT(0));
        return;
    }

    const Value v = vm.stack_manager.pop();
    if (v.type == ValueType::OBJECT && v.current_value.object) {
        if (auto* arr = dynamic_cast<ObjArray*>(v.current_value.object)) {
            vm.stack_manager.push(Value::createINT(static_cast<int64_t>(arr->value.size())));
            return;
        }
        if (auto* map = dynamic_cast<ObjMap*>(v.current_value.object)) {
            vm.stack_manager.push(Value::createINT(static_cast<int64_t>(map->value.size())));
            return;
        }
        if (auto* str = dynamic_cast<ObjString*>(v.current_value.object)) {
            vm.stack_manager.push(Value::createINT(static_cast<int64_t>(str->value.size())));
            return;
        }
    }
    vm.stack_manager.push(Value::createINT(0));
}

// Native int function
void native_int(VM& vm, const uint8_t argc) {
    if (argc != 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createINT(0));
        return;
    }

    Value v = vm.stack_manager.pop();
    try {
        int64_t result = std::stoll(v.toString());
        vm.stack_manager.push(Value::createINT(result));
    } catch (...) {
        vm.stack_manager.push(Value::createINT(0));
    }
}

// Native float function
void native_float(VM& vm, const uint8_t argc) {
    if (argc != 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createDOUBLE(0.0));
        return;
    }

    Value v = vm.stack_manager.pop();
    try {
        double result = std::stod(v.toString());
        vm.stack_manager.push(Value::createDOUBLE(result));
    } catch (...) {
        vm.stack_manager.push(Value::createDOUBLE(0.0));
    }
}

// Native float function
void native_exit(VM& vm, const uint8_t argc) {
    if (argc != 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createDOUBLE(0.0));
        return;
    }

    Value v = vm.stack_manager.pop();
    try {
        const int result = std::stoi(v.toString());
        exit(result);
    } catch (...) {
        exit(1);
    }
}

// Native input function
void native_input(VM& vm, const uint8_t argc) {
    std::string line;

    if (argc == 1) {
        Value prompt = vm.stack_manager.pop();
        std::cout << prompt.toString();
    } else if (argc > 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    std::getline(std::cin, line);
    ObjString* str = vm.allocator.allocate_string(line);
    vm.stack_manager.push(Value::createOBJECT(str));
}
