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
 *  File: StackManager
 *  Created: 11/9/2025
 * ============================================================
 */
#include "StackManager.h"

#include <iostream>

#include "VM.h"

void StackManager::push(const Value &value) {
    if (sp >= stack.size()) {
        stack.push_back(value);
    } else {
        stack[sp] = value;
    }
    sp++;
}

Value StackManager::pop() {
    if (sp == 0) return Value::createNIL();
    sp--;
    return stack[sp];
}

Value StackManager::peek(const int position) const {
    const int idx = static_cast<int>(sp) - 1 - position;
    if (idx < 0) return Value::createNIL();
    return stack[idx];
}