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
#ifndef DROPLET_STACKMANAGER_H
#define DROPLET_STACKMANAGER_H
#include <cstdint>
#include <vector>


struct Value;

struct StackManager {
    std::vector<Value> stack;
    uint32_t sp = 0;

    // ops
    void push(const Value &value);
    Value pop();
    Value peek(int position) const;
};


#endif //DROPLET_STACKMANAGER_H