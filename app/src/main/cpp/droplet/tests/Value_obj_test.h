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
 *  File: Value_obj_test
 *  Created: 11/8/2025
 * ============================================================
 */
#ifndef DROPLET_VALUE_OBJ_TEST_H
#define DROPLET_VALUE_OBJ_TEST_H
#include <iostream>

#include "../vm/Value.h"
#include "../vm/Object.h"

inline std::string print_value_str(const ValueType type) {
    switch (type) {
        case ValueType::NIL:     return "NIL";
        case ValueType::BOOL:    return "BOOL";
        case ValueType::INT:     return "INT";
        case ValueType::DOUBLE:  return "DOUBLE";
        case ValueType::OBJECT:  return "OBJECT";
        default:                 return "UNKNOWN";
    }
}

inline void _print_val_info(const Value &value) {
    std::cout<<"val: "<<value.toString()<<"\n";
    std::cout<<"type: "<<print_value_str(value.type)<<"\n";
    std::cout<<"value(b): "<<value.current_value.b<<"\n";
    std::cout<<"value(i): "<<value.current_value.i<<"\n";
    std::cout<<"value(d): "<<value.current_value.d<<"\n";
    std::cout<<"value(o): "<<value.current_value.object<<"\n";
    std::cout<<"is true: "<<value.isTruthy()<<"\n";
    std::cout<<"\n\n\n"<<std::endl;
}

inline void TEST_VALUE_OBJECT() {
    // null
    const Value v = Value::createNIL();
    _print_val_info(v);

    // bool - true
    const Value v1 = Value::createBOOL(true);
    _print_val_info(v1);

    // bool - false
    const Value v1f = Value::createBOOL(false);
    _print_val_info(v1f);

    // double
    const Value v2 = Value::createDOUBLE(1.5);
    _print_val_info(v2);

    // double - 0
    const Value v20 = Value::createDOUBLE(0);
    _print_val_info(v20);

    // object
    auto *o1 = new ObjString("Hello");
    const Value v3 = Value::createOBJECT(o1);
    _print_val_info(v3);


    // int
    const Value i2 = Value::createINT(1);
    _print_val_info(i2);

    // double - 0
    const Value i20 = Value::createINT(0);
    _print_val_info(i20);
}

#endif //DROPLET_VALUE_OBJ_TEST_H