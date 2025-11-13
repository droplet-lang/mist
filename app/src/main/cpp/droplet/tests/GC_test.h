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
 *  File: GC_test
 *  Created: 11/8/2025
 * ============================================================
 */
#ifndef GC_TEST_I
#define GC_TEST_I


#define MEM_THRESHOLD_FOR_NEXT_GC_CALL 20


#include "../vm/GC.h"

#include <assert.h>
#include <iostream>

inline void TEST_GC() {
    GC gc;

    auto *o1 = new ObjString("Hello");
    auto *o2 = new ObjString("World");
    gc.allocNewObject(o1);
    gc.allocNewObject(o2);

    // ensure alloc worked
    assert(gc.heap.size() == 2);
    std::cout<<"Alloc worked\n";

    // simulate marking reachable object
    const RWComplexGCFunction root_walker = [&](const MarkerFunction &mark) {
        mark(Value::createOBJECT(o1));
    };

    gc.collect(root_walker);

    // ensure collect worked
    assert(gc.heap.size() == 1);
    assert(gc.heap[0] == o1);
    std::cout<<"Collect worked\n";
}

#endif