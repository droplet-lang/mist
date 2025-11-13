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
 *  File: GC
 *  Created: 11/7/2025
 * ============================================================
 */
#include "GC.h"

void GC::allocNewObject(Object *obj) {
    heap.push_back(obj);
}

void GC::markAll(const RWComplexGCFunction &rootWalker) {
    rootWalker([this](const Value v) {
        markValue(v);
    });
}

void GC::markValue(const Value value) {
    // we should only mark objects, non objject type should not be GC
    if (value.type != ValueType::OBJECT) return;

    Object *obj = value.current_value.object;
    if (!obj) return;

    if (obj->marked) return;

    // Given a value,mark it and recursively do it for all the children
    obj->marked = true;
    obj->markChildren([this](const Value v) {
        markValue(v);
    });

#ifndef MEM_ADJUST_THRESHOLD_ON_EVERY_GC_CALL
    // only increase size if explicit
    memThresholdForNextGcCall = std::max<size_t>(MEM_THRESHOLD_FOR_NEXT_GC_CALL, heap.size() * 2);
#endif
}

void GC::sweep() {
    std::vector<Object *> newHeap;
    newHeap.reserve(heap.size());

    for (Object *p: heap) {
        if (!p->marked) {
            delete p;
        } else {
            p->marked = false;
            newHeap.push_back(p);
        }
    }

    heap.swap(newHeap);
}

void GC::collectIfNeeded(const RWComplexGCFunction &rootWalker) {
    if (heap.size() >= memThresholdForNextGcCall) {
        collect(rootWalker);
    }
}

void GC::collect(const RWComplexGCFunction &rootWalker) {
    markAll([&rootWalker](const MarkerFunction &mark) {
        rootWalker(mark);
    });
    sweep();
}
