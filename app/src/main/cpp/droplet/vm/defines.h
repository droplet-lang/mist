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
 *  File: defines
 *  Created: 11/7/2025
 * ============================================================
 */
#ifndef DROPLET_DEFINES_H
#define DROPLET_DEFINES_H
#include <functional>

#include "Value.h"

struct VM;

typedef std::function<void(Value)> MarkerFunction;
typedef std::function<void(MarkerFunction)> RWComplexGCFunction;
typedef std::function<void(VM&, int)> NativeFunction;

// operations (see /docs/vm.md for design)
// Seperated each type into multiple of 10 group (easy for future debug)
enum Op: uint8_t {
    // Stack
    OP_PUSH_CONST = 0x01,
    OP_POP = 0x02,
    OP_LOAD_LOCAL = 0x03,
    OP_STORE_LOCAL = 0x04,
    OP_DUP = 0x05,
    OP_SWAP = 0x06,
    OP_ROT = 0x07,

    // Arithmetic
    OP_ADD = 0x10,
    OP_SUB = 0x11,
    OP_MUL = 0x12,
    OP_DIV = 0x13,
    OP_MOD = 0x14,

    // Logical
    OP_AND = 0x20,
    OP_OR  = 0x21,
    OP_NOT = 0x22,

    // Comparison
    OP_EQ  = 0x30,
    OP_NEQ = 0x31,
    OP_LT  = 0x32,
    OP_GT  = 0x33,
    OP_LTE = 0x34,
    OP_GTE = 0x35,
    OP_IS_INSTANCE = 0x36,

    // Control flow
    OP_JUMP = 0x40,
    OP_JUMP_IF_FALSE = 0x41,
    OP_JUMP_IF_TRUE = 0x42,

    // Calls
    OP_CALL = 0x50,
    OP_RETURN = 0x51,
    OP_CALL_NATIVE = 0x52,
    OP_CALL_FFI = 0x53,

    // Object ops
    OP_NEW_OBJECT = 0x60,
    OP_GET_FIELD = 0x61,
    OP_SET_FIELD = 0x62,
    OP_NEW_ARRAY = 0x63,
    OP_NEW_MAP = 0x64,

    // Array ops
    OP_ARRAY_GET = 0x70,
    OP_ARRAY_SET = 0x71,

    // Map ops
    OP_MAP_SET = 0x80,
    OP_MAP_GET = 0x81,

    // String ops
    OP_STRING_CONCAT = 0x90,
    OP_STRING_LENGTH = 0x91,
    OP_STRING_SUBSTR = 0x92,
    OP_STRING_EQ = 0x93,
    OP_STRING_GET_CHAR = 0x94,

    // Globals
    OP_LOAD_GLOBAL = 0xA0,
    OP_STORE_GLOBAL = 0xA1
};

#endif //DROPLET_DEFINES_H