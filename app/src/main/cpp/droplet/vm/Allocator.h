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
#ifndef DROPLET_ALLOCATOR_H
#define DROPLET_ALLOCATOR_H
#include "GC.h"
#include "StackManager.h"


struct Allocator {
    // GC
    GC gc;

    // allocators
    ObjString *allocate_string(const std::string &str);
    ObjArray *allocate_array();
    ObjMap *allocate_map();
    ObjInstance *allocate_instance(const std::string &className);

    // stack root walker for GC
    void root_walker(const RWComplexGCFunction &walker);
    void collect_garbage_if_needed(StackManager &sm, std::unordered_map<std::string, Value> &globals);
    void perform_gc(StackManager &stack, std::unordered_map<std::string, Value> &globals);
};


#endif //DROPLET_ALLOCATOR_H