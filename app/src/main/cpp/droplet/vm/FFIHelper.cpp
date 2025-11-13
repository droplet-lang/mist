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
 *  File: FFI
 *  Created: 11/9/2025
 * ============================================================
 */
#include "FFIHelper.h"

#include <iostream>
#include <vector>

#include "Value.h"
#include "../external/dlfcn/dlfcn.h"
#include "Object.h"
#include "VM.h"

//FFISignature parseSignature(const std::string& sig) {
//    FFISignature parsed;
//    size_t arrow = sig.find("->");
//    std::string argsPart = sig.substr(0, arrow);
//    std::string retPart = sig.substr(arrow + 2);
//
//    // remove commas and spaces
//    for (char c : argsPart) {
//        if (std::isalpha(c))
//            parsed.argTypes.push_back(c);
//    }
//
//    parsed.returnType = retPart.empty() ? 'v' : retPart[0];
//    return parsed;
//}
//
//void *FFIHelper::load_lib(const std::string &path) {
//    auto it = libs.find(path);
//    if (it != libs.end()) return it->second;
//
//
//    void *h = dlopen(path.c_str(), RTLD_NOW);
//    if (!h) {
//        std::cerr << "[FFI] dlopen failed: " << dlerror() << '\n';
//        return nullptr;
//    }
//
//    libs[path] = h;
//    return h;
//}
//
//void *FFIHelper::find_symbol(void *lib, const std::string &symbol) {
//    if (!lib) return nullptr;
//
//    void *p = dlsym(lib, symbol.c_str());
//    return p;
//}
//
//ffi_type* FFIHelper::get_ffi_type(const char t) {
//    switch(t) {
//        case 'i': return &ffi_type_sint64;
//        case 'f': return &ffi_type_double;
//        case 'b': return &ffi_type_uint8; // boolean as byte
//        case 's': return &ffi_type_pointer; // string as char*
//        default: return &ffi_type_void;
//    }
//}


Value
FFIHelper::do_ffi_call(const std::string &sig, std::vector<Value> &args, VM *vm, void *fn_ptr) {
//    FFISignature s = parseSignature(sig);

    // Storage for libffi
    // Preallocate fixed-size storage to prevent reallocation
    std::vector<ffi_type *> argTypes;
    std::vector<void *> argValues;
    std::vector<int64_t> intStorage(args.size());
    std::vector<double> doubleStorage(args.size());
    std::vector<const char *> stringStorage(args.size());

    size_t intIdx = 0, doubleIdx = 0, strIdx = 0;

    // Convert VM Values to libffi pointers
//    for (size_t i = 0; i < args.size(); i++) {
//        auto &v = args[i];
//        switch (v.type) {
//            case ValueType::INT:
//                intStorage[intIdx] = v.current_value.i;
//                argValues.push_back(&intStorage[intIdx]);
////                argTypes.push_back(&ffi_type_sint64);
//                intIdx++;
//                break;
//            case ValueType::DOUBLE:
//                doubleStorage[doubleIdx] = v.current_value.d;
//                argValues.push_back(&doubleStorage[doubleIdx]);
//                argTypes.push_back(&ffi_type_double);
//                doubleIdx++;
//                break;
//            case ValueType::BOOL:
//                intStorage[intIdx] = v.current_value.b ? 1 : 0;
//                argValues.push_back(&intStorage[intIdx]);
//                argTypes.push_back(&ffi_type_uint8);
//                intIdx++;
//                break;
//            default:
//                argValues.push_back(nullptr);
//                argTypes.push_back(&ffi_type_void);
//                break;
//        }
//    }

//    ffi_cif cif;
//    ffi_type* retType = get_ffi_type(s.returnType);
//    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, argTypes.size(), retType, argTypes.data()) != FFI_OK) {
//        return Value::createNIL();
//    }

    // Storage for return value
    int64_t retInt = 0;
    double retDouble = 0.0;
    bool retBool = false;
    const char *retStr = nullptr;
    void *retAddr = nullptr;

//    switch (s.returnType) {
//        case 'i': retAddr = &retInt; break;
//        case 'f': retAddr = &retDouble; break;
//        case 'b': retAddr = &retBool; break;
//        case 's': retAddr = &retStr; break;
//        default: retAddr = nullptr; break;
//    }

//    ffi_call(&cif, FFI_FN(fn_ptr), retAddr, argValues.data());

//    switch (s.returnType) {
//        case 'i': return Value::createINT(retInt);
//        case 'f': return Value::createDOUBLE(retDouble);
//        case 'b': return Value::createBOOL(retBool);
//        case 's': {
//            ObjString* strVal = vm->allocator.allocate_string(std::string(retStr));
//            return Value::createOBJECT(strVal);
//        }
//        default: return Value::createNIL();
//    }
    return Value::createNIL();
}

