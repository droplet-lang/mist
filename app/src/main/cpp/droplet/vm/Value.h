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
 *  File: Value
 *  Created: 11/7/2025
 * ============================================================
 */
#ifndef DROPLET_VALUE_H
#define DROPLET_VALUE_H
#include <cstdint>
#include <string>

struct Object;

enum class ValueType {
    NIL = 0,
    BOOL = 1,
    INT = 2,
    DOUBLE = 3,
    OBJECT = 4
};

struct Value {
    ValueType type = ValueType::NIL;

    // NOTE: initially everything will be zero/null.
    // This will later affect the runtime.
    // uninitiated vars are 0/false/null.
    union {
        bool b;
        int64_t i; // int does not have fixed size, so let's use int(64)
        double d;
        Object *object;
    } current_value{};

    static Value createNIL();

    static Value createBOOL(bool v);

    static Value createINT(int v);

    static Value createDOUBLE(double v);

    static Value createOBJECT(Object* v);

    [[nodiscard]] std::string toString() const;

    [[nodiscard]] bool isTruthy() const;
};


#endif //DROPLET_VALUE_H