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
 *  File: Function
 *  Created: 11/9/2025
 * ============================================================
 */
#ifndef DROPLET_FUNCTION_H
#define DROPLET_FUNCTION_H
#include <cstdint>
#include <string>
#include <vector>


struct Value;

struct Function {
    std::string name;
    std::vector<Value> constants;
    std::vector<uint8_t> code;
    uint8_t argCount = 0;
    uint8_t localCount = 0; // will include n(local_slots) + args
};



#endif //DROPLET_FUNCTION_H