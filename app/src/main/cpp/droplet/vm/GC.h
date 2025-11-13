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
#ifndef DROPLET_GC_H
#define DROPLET_GC_H
#include <functional>
#include <vector>

#include "defines.h"
#include "Object.h"
#include "Value.h"

#ifndef MEM_THRESHOLD_FOR_NEXT_GC_CALL
    // Todo (@svpz) We can even let user fine tune this???
    #define MEM_THRESHOLD_FOR_NEXT_GC_CALL 1024 * 1024
#endif


// For current implementation, we will use normal mark-sweep
// based Garbage Collector, refinement is always welcome
// ref https://www.geeksforgeeks.org/java/mark-and-sweep-garbage-collection-algorithm/
struct GC {
    std::vector<Object*> heap;
    size_t memThresholdForNextGcCall = MEM_THRESHOLD_FOR_NEXT_GC_CALL;

    // Simply push the given object into the HEAP, no rocket science here
    void allocNewObject(Object *obj);

    // Goes through the list of children(rootwalker function) and mark that value
    // Root walker basically does:
    //  a. Go through the object in Stack and Globals
    //  b. Mark those object and their children recursively (as they are reachable)
    void markAll(const RWComplexGCFunction &rootWalker);

    // If the given value is an Object, do:
    // a. get the associated object of this value
    // b. if the object is not null and is not marked,
    //    mark this object and recursively mark all the children of this object
    void markValue(Value value);

    // loop through the heap, delete the unmarked object
    // set the marked object to unmarked and add to new Heap which replaces old heap
    void sweep();

    // only collect if collection threshold is reached, calls the collect method
    void collectIfNeeded(const RWComplexGCFunction &rootWalker);

    // run a markAll and sweep once
    void collect(const RWComplexGCFunction &rootWalker);
};


#endif //DROPLET_GC_H