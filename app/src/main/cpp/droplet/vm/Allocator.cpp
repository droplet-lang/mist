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
 *  File: Allocator
 *  Created: 11/9/2025
 * ============================================================
 */
#include "Allocator.h"

#include "StackManager.h"

ObjString *Allocator::allocate_string(const std::string &str) {
    auto *o = new ObjString(str);
    gc.allocNewObject(o);
    return o;
}

ObjArray *Allocator::allocate_array() {
    auto *o = new ObjArray();
    gc.allocNewObject(o);
    return o;
}

ObjMap *Allocator::allocate_map() {
    auto *o = new ObjMap();
    gc.allocNewObject(o);
    return o;
}

ObjInstance *Allocator::allocate_instance(const std::string &className) {
    auto *o = new ObjInstance(className);
    gc.allocNewObject(o);
    return o;
}

void Allocator::root_walker(const RWComplexGCFunction &walker) {
    walker([this](const Value v) {
        gc.markValue(v);
    });
}

void Allocator::collect_garbage_if_needed(StackManager &sm, std::unordered_map<std::string, Value> &globals) {
    if (gc.heap.size() > gc.memThresholdForNextGcCall) {
        perform_gc(sm, globals);
    }
}

void Allocator::perform_gc(StackManager &stack, std::unordered_map<std::string, Value> &globals) {
    gc.collect([stack, globals](const MarkerFunction &mark) {
        // stack (frame ko local are already part of stack)
        // so this must cover stack + frame local
        for (uint32_t i = 0; i < stack.sp; i++)
            mark(stack.stack[i]);

        // globals
        for (const auto &val: globals)
            mark(val.second);
        // function constants (if we used global constants)
        // nothing else
    });
}