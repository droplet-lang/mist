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
 *  File: VM
 *  Created: 11/7/2025
 * ============================================================
 */
#include "VM.h"
#include "../debugger/Debugger.h"

#include <cmath>
#include <fstream>
#include <iostream>


void VM::do_return(const uint8_t return_count) {
    if (call_frames.empty()) return;

    // Collect return values
    std::vector<Value> rets;
    for (int i = 0; i < return_count; i++) {
        rets.push_back(stack_manager.pop());
    }

    const CallFrame frame = call_frames.back();
    call_frames.pop_back();

    // CORRECT FIX: The returning function's locals start at frame.localStartsAt
    // We need to restore sp_internal to that position, which will:
    // 1. Remove all the function's locals (arguments + additional locals)
    // 2. Leave the caller's stack intact (because localStartsAt points to where
    //    this function's data begins, which is AFTER the caller's data)

    const uint32_t restore = frame.localStartsAt;

    // Restore stack pointer to before this function's locals
    stack_manager.sp = restore;


    // Push return values
    for (int i = static_cast<int>(rets.size()) - 1; i >= 0; i--) {
        stack_manager.push(rets[i]);
    }
}

uint32_t VM::get_function_index(const std::string &name) {
    const auto it = function_index_by_name.find(name);
    if (it == function_index_by_name.end()) return UINT32_MAX;
    return it->second;
}

void VM::call_function_by_index(const uint32_t fnIndex, size_t argCount) {
    if (fnIndex >= functions.size()) {
        std::cerr << "#########call_function_by_index: bad index\n";
        return;
    }

    Function *f = functions[fnIndex].get();
    CallFrame frame;
    frame.function = f;
    frame.ip = 0;
    frame.localStartsAt = stack_manager.sp;
    call_frames.push_back(frame);
}

void VM::register_native(const std::string &name, NativeFunction fn) {
    native_functions_registry[name] = fn;
}

void VM::run() {
    // GC GC GC
    while (!call_frames.empty()) {
        allocator.collect_garbage_if_needed(stack_manager, globals);

        // DEBUG MODE: Check if we should break
        if (debugMode && debugger && debugger->shouldBreak()) {
            debugger->pause();
            debugger->debugLoop();
            if (!debugger->isRunning) {
                return;  // User quit debugger
            }
        }

        CallFrame &frame = call_frames.back();
        Function *func = frame.function;
        if (frame.ip >= func->code.size()) {
            do_return(0);
            continue;
        }

        uint8_t op = frame.read_u8();
        switch (static_cast<Op>(op)) {
            case OP_PUSH_CONST: {
                const uint32_t idx = frame.read_u32();
                if (idx >= global_constants.size()) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                stack_manager.push(global_constants[idx]);
                break;
            }
            case OP_POP: {
                stack_manager.pop();
                break;
            }
            case OP_CALL: {
                uint32_t fnIdx = frame.read_u32();
                uint8_t argc = frame.read_u8();

                if (fnIdx >= functions.size()) {
                    // pop args
                    for (int i = 0; i < argc; i++) stack_manager.pop();
                    stack_manager.push(Value::createNIL());
                    break;
                }

                Function *callee = functions[fnIdx].get();

                // The arguments are on the stack at positions [sp_internal - argc .. sp_internal - 1]
                // These arguments become the first 'argc' locals of the called function
                // localStartsAt points to where local 0 begins (which is the first argument)

                CallFrame newFrame;
                newFrame.function = callee;
                newFrame.ip = 0;
                newFrame.localStartsAt = stack_manager.sp - argc;


                // CRITICAL FIX: Ensure we have space for all locals (not just args)
                // The function might have more locals than just the arguments
                // We need to allocate space for localCount locals total

                // Arguments occupy slots [0 .. argc-1]
                // Additional locals occupy slots [argc .. localCount-1]
                uint8_t additionalLocals = callee->localCount > argc ? callee->localCount - argc : 0;


                // Push NIL for additional local variable slots
                for (uint8_t i = 0; i < additionalLocals; i++) {
                    stack_manager.push(Value::createNIL());
                }

                call_frames.push_back(newFrame);
                break;
            }

            case OP_LOAD_LOCAL: {
                uint8_t slot = frame.read_u8();
                uint32_t abs = frame.localStartsAt + slot;


                if (abs < stack_manager.sp) {
                    Value val = stack_manager.stack[abs];
                    stack_manager.push(val);
                } else {
                    stack_manager.push(Value::createNIL());
                }
                break;
            }
            case OP_STORE_LOCAL: {
                uint8_t slot = frame.read_u8();
                uint32_t abs = frame.localStartsAt + slot;
                Value val = stack_manager.pop();
                // ensure stack has space
                while (stack_manager.sp <= abs)
                    stack_manager.push(Value::createNIL());
                stack_manager.stack[abs] = val;
                break;
            }
            case OP_DUP: {
                Value v = stack_manager.peek(0);
                stack_manager.push(v);
                break;
            }
            case OP_SWAP: {
                Value a = stack_manager.pop();
                Value b = stack_manager.pop();
                stack_manager.push(a);
                stack_manager.push(b);
                break;
            }
            case OP_ROT: {
                // rotate top 3: a b c -> b c a
                Value a = stack_manager.pop();
                Value b = stack_manager.pop();
                Value c = stack_manager.pop();
                stack_manager.push(b);
                stack_manager.push(a);
                stack_manager.push(c);
                break;
            }

            // Arithmetic
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD: {
                Value vb = stack_manager.pop();
                Value va = stack_manager.pop();
                // numeric coercion
                bool aDouble = (va.type == ValueType::DOUBLE), bDouble = (vb.type == ValueType::DOUBLE);
                double da = (va.type == ValueType::DOUBLE)
                                ? va.current_value.d
                                : (va.type == ValueType::INT ? static_cast<double>(va.current_value.i) : 0.0);
                double db = (vb.type == ValueType::DOUBLE)
                                ? vb.current_value.d
                                : (vb.type == ValueType::INT ? static_cast<double>(vb.current_value.i) : 0.0);
                double res = 0;
                if (static_cast<Op>(op) == OP_ADD) res = da + db;
                if (static_cast<Op>(op) == OP_SUB) res = da - db;
                if (static_cast<Op>(op) == OP_MUL) res = da * db;
                if (static_cast<Op>(op) == OP_DIV) res = da / db;
                if (static_cast<Op>(op) == OP_MOD) res = fmod(da, db);
                // if both ints and not DIV -> int
                if (!aDouble && !bDouble && static_cast<Op>(op) != OP_DIV)
                    stack_manager.push(
                        Value::createINT(static_cast<int64_t>(res)));
                else stack_manager.push(Value::createDOUBLE(res));
                break;
            }

            // Logical
            case OP_AND: {
                Value vb = stack_manager.pop();
                Value va = stack_manager.pop();
                bool av = va.isTruthy();
                bool bv = vb.isTruthy();
                stack_manager.push(Value::createBOOL(av && bv));
                break;
            }
            case OP_OR: {
                Value vb = stack_manager.pop();
                Value va = stack_manager.pop();
                bool av = va.isTruthy();
                bool bv = vb.isTruthy();
                stack_manager.push(Value::createBOOL(av || bv));
                break;
            }
            case OP_NOT: {
                Value a = stack_manager.pop();
                stack_manager.push(Value::createBOOL(!a.isTruthy()));
                break;
            }

            // Comparison
            case OP_EQ:
            case OP_NEQ:
            case OP_LT:
            case OP_GT:
            case OP_LTE:
            case OP_GTE: {
                Value vb = stack_manager.pop();
                Value va = stack_manager.pop();
                bool res = false;
                // numeric or string or identity
                if ((va.type == ValueType::INT || va.type == ValueType::DOUBLE) && (
                        vb.type == ValueType::INT || vb.type == ValueType::DOUBLE)) {
                    double da = (va.type == ValueType::DOUBLE)
                                    ? va.current_value.d
                                    : static_cast<double>(va.current_value.i);
                    double db = (vb.type == ValueType::DOUBLE)
                                    ? vb.current_value.d
                                    : static_cast<double>(vb.current_value.i);
                    if (static_cast<Op>(op) == OP_EQ) res = (da == db);
                    if (static_cast<Op>(op) == OP_NEQ) res = (da != db);
                    if (static_cast<Op>(op) == OP_LT) res = (da < db);
                    if (static_cast<Op>(op) == OP_GT) res = (da > db);
                    if (static_cast<Op>(op) == OP_LTE) res = (da <= db);
                    if (static_cast<Op>(op) == OP_GTE) res = (da >= db);
                } else if (va.type == ValueType::OBJECT && vb.type == ValueType::OBJECT) {
                    // if both strings, compare strings
                    auto *sa = dynamic_cast<ObjString *>(va.current_value.object);
                    auto sb = dynamic_cast<ObjString *>(vb.current_value.object);
                    if (sa && sb) {
                        if (static_cast<Op>(op) == OP_EQ) res = (sa->value == sb->value);
                        if (static_cast<Op>(op) == OP_NEQ) res = (sa->value != sb->value);
                        if (static_cast<Op>(op) == OP_LT) res = (sa->value < sb->value);
                        if (static_cast<Op>(op) == OP_GT) res = (sa->value > sb->value);
                        if (static_cast<Op>(op) == OP_LTE) res = (sa->value <= sb->value);
                        if (static_cast<Op>(op) == OP_GTE) res = (sa->value >= sb->value);
                    } else {
                        // fallback identity
                        if (static_cast<Op>(op) == OP_EQ) res = (va.current_value.object == vb.current_value.object);
                        if (static_cast<Op>(op) == OP_NEQ) res = (va.current_value.object != vb.current_value.object);
                    }
                } else {
                    // fallback equality by type+value for int/double/bool
                    if (static_cast<Op>(op) == OP_EQ) res = (va.type == vb.type && va.toString() == vb.toString());
                    if (static_cast<Op>(op) == OP_NEQ) res = !(va.type == vb.type && va.toString() == vb.toString());
                }
                stack_manager.push(Value::createBOOL(res));
                break;
            }

            // Control flow
            case OP_JUMP: {
                uint32_t target = frame.read_u32();
                frame.ip = target;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint32_t target = frame.read_u32();
                Value cond = stack_manager.pop();
                if (!cond.isTruthy()) frame.ip = target;
                break;
            }
            case OP_JUMP_IF_TRUE: {
                uint32_t target = frame.read_u32();
                Value cond = stack_manager.pop();
                if (cond.isTruthy()) frame.ip = target;
                break;
            }
            case OP_RETURN: {
                uint8_t retCount = frame.read_u8();
                do_return(retCount);
                break;
            }
            case OP_CALL_NATIVE: {
                uint32_t nameIdx = frame.read_u32();
                uint8_t argc = frame.read_u8();

                if (nameIdx >= global_constants.size()) {
                    std::cerr << "CALL_NATIVE: bad nameIdx\n";
                    for (int i = 0; i < argc; i++) stack_manager.pop();
                    stack_manager.push(Value::createNIL());
                    break;
                }

                Value nameVal = global_constants[nameIdx];

                auto *on = dynamic_cast<ObjString *>(nameVal.current_value.object);
                if (!on) {
                    std::cerr << "CALL_NATIVE: name not string\n";
                    for (int i = 0; i < argc; i++) stack_manager.pop();
                    stack_manager.push(Value::createNIL());
                    break;
                }

                std::string nativeName = on->value;
                auto it = native_functions_registry.find(nativeName);
                if (it == native_functions_registry.end()) {
                    std::cerr << "CALL_NATIVE: not found " << nativeName << "\n";
                    for (int i = 0; i < argc; i++) stack_manager.pop();
                    stack_manager.push(Value::createNIL());
                } else {
                    it->second(*this, argc);
                }
                break;
            }
            case OP_CALL_FFI: {
                uint32_t libIdx = frame.read_u32();
                uint32_t symIdx = frame.read_u32();
                uint8_t argc = frame.read_u8();
                uint32_t sigIdx = frame.read_u32();

                if (libIdx >= global_constants.size() ||
                    symIdx >= global_constants.size() ||
                    sigIdx >= global_constants.size()) {
                    std::cerr << "CALL_FFI: Invalid constant indices\n";
                    for (int i = 0; i < argc; i++)
                        stack_manager.pop();
                    stack_manager.push(Value::createNIL());
                    break;
                }

                auto *libNameObj = dynamic_cast<ObjString *>(global_constants[libIdx].current_value.object);
                auto *symNameObj = dynamic_cast<ObjString *>(global_constants[symIdx].current_value.object);
                auto* sigObj = dynamic_cast<ObjString*>(global_constants[sigIdx].current_value.object);

                if (!libNameObj || !symNameObj || !sigObj) {
                    std::cerr << "CALL_FFI: Invalid string objects\n";
                    for (int i = 0; i < argc; i++)
                        stack_manager.pop();
                    stack_manager.push(Value::createNIL());
                    break;
                }

//                void *h = ffi.load_lib(libNameObj->value);
//                if (!h) {
                for (int i = 0; i < argc; i++)
                    stack_manager.pop();
                stack_manager.push(Value::createNIL());
                break;
//                }

//                void *sym = FFIHelper::find_symbol(h, symNameObj->value);
//                if (!sym) {
//                    std::cerr << "CALL_FFI: symbol missing\n";
//                    for (int i = 0; i < argc; i++)
//                        stack_manager.pop();
//                    stack_manager.push(Value::createNIL());
//                    break;
//                }
//
//                std::vector<Value> args;
//                args.reserve(argc);
//                for (int i = 0; i < argc; i++) {
//                    args.insert(args.begin(), stack_manager.pop());
//                }
//
//                Value ffi_response = FFIHelper::do_ffi_call(sigObj->value, args, this, sym);
//                stack_manager.push(ffi_response);
//                break;
            }
            case OP_NEW_OBJECT: {
                uint32_t nameIdx = frame.read_u32();
                if (nameIdx >= global_constants.size()) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                auto *on = dynamic_cast<ObjString *>(global_constants[nameIdx].current_value.object);
                std::string className = on ? on->value : std::string("Object");
                ObjInstance *inst = allocator.allocate_instance(className);
                stack_manager.push(Value::createOBJECT(inst));
                break;
            }
            case OP_IS_INSTANCE: {
                uint32_t typeIdx = frame.read_u32();
                Value objVal = stack_manager.pop();

                if (typeIdx >= global_constants.size()) {
                    stack_manager.push(Value::createBOOL(false));
                    break;
                }

                auto *typeName = dynamic_cast<ObjString *>(global_constants[typeIdx].current_value.object);
                if (!typeName) {
                    stack_manager.push(Value::createBOOL(false));
                    break;
                }

                if (objVal.type != ValueType::OBJECT || !objVal.current_value.object) {
                    stack_manager.push(Value::createBOOL(false));
                    break;
                }

                auto *instance = dynamic_cast<ObjInstance *>(objVal.current_value.object);
                if (!instance) {
                    stack_manager.push(Value::createBOOL(false));
                    break;
                }

                // Check exact type match
                bool matches = (instance->className == typeName->value);

                stack_manager.push(Value::createBOOL(matches));
                break;
            }
            case OP_GET_FIELD: {
                uint32_t nameIdx = frame.read_u32();
                Value objv = stack_manager.pop();

                if (objv.type != ValueType::OBJECT || !objv.current_value.object) {
                    stack_manager.push(Value::createNIL());
                    break;
                }

                // Get the field name from constants
                if (nameIdx >= global_constants.size()) {
                    stack_manager.push(Value::createNIL());
                    break;
                }

                auto *on = dynamic_cast<ObjString *>(global_constants[nameIdx].current_value.object);
                if (!on) {
                    stack_manager.push(Value::createNIL());
                    break;
                }

                auto *oi = dynamic_cast<ObjInstance *>(objv.current_value.object);
                if (!oi) {
                    stack_manager.push(Value::createNIL());
                    break;
                }

                auto it = oi->fields.find(on->value);
                if (it == oi->fields.end()) {
                    stack_manager.push(Value::createNIL());
                } else {
                    stack_manager.push(it->second);
                }
                break;
            }
            case OP_SET_FIELD: {
                uint32_t nameIdx = frame.read_u32();
                Value val = stack_manager.pop();
                Value objv = stack_manager.pop();
                if (objv.type != ValueType::OBJECT || !objv.current_value.object) break;
                auto on = dynamic_cast<ObjString *>(global_constants[nameIdx].current_value.object);
                if (!on) break;
                auto *oi = dynamic_cast<ObjInstance *>(objv.current_value.object);
                if (!oi) break;
                oi->fields[on->value] = val;
                break;
            }

            // Array
            case OP_ARRAY_GET: {
                Value idxv = stack_manager.pop();
                Value arrv = stack_manager.pop();
                if (arrv.type != ValueType::OBJECT || !arrv.current_value.object) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                auto *oa = dynamic_cast<ObjArray *>(arrv.current_value.object);
                if (!oa) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                int idx = (idxv.type == ValueType::INT)
                              ? static_cast<int>(idxv.current_value.i)
                              : (idxv.type == ValueType::DOUBLE ? static_cast<int>(idxv.current_value.d) : 0);
                if (idx < 0 || idx >= static_cast<int>(oa->value.size())) stack_manager.push(Value::createNIL());
                else stack_manager.push(oa->value[idx]);
                break;
            }
            case OP_ARRAY_SET: {
                Value val = stack_manager.pop();
                Value idxv = stack_manager.pop();
                Value arrv = stack_manager.pop();
                if (arrv.type != ValueType::OBJECT || !arrv.current_value.object) break;
                auto oa = dynamic_cast<ObjArray *>(arrv.current_value.object);
                if (!oa) break;
                int idx = (idxv.type == ValueType::INT)
                              ? static_cast<int>(idxv.current_value.i)
                              : (idxv.type == ValueType::DOUBLE ? static_cast<int>(idxv.current_value.d) : 0);
                if (idx < 0) break;
                if (idx >= static_cast<int>(oa->value.size())) oa->value.resize(idx + 1, Value::createNIL());
                oa->value[idx] = val;
                break;
            }

            // Map
            case OP_MAP_SET: {
                Value val = stack_manager.pop();
                Value keyv = stack_manager.pop();
                Value mapv = stack_manager.pop();
                if (mapv.type != ValueType::OBJECT || !mapv.current_value.object) break;
                auto om = dynamic_cast<ObjMap *>(mapv.current_value.object);
                if (!om) break;
                std::string key = keyv.toString();
                om->value[key] = val;
                break;
            }
            case OP_MAP_GET: {
                Value keyv = stack_manager.pop();
                Value mapv = stack_manager.pop();
                if (mapv.type != ValueType::OBJECT || !mapv.current_value.object) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                auto om = dynamic_cast<ObjMap *>(mapv.current_value.object);
                if (!om) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                std::string key = keyv.toString();
                auto it = om->value.find(key);
                if (it == om->value.end()) stack_manager.push(Value::createNIL());
                else stack_manager.push(it->second);
                break;
            }

            // String ops
            case OP_STRING_CONCAT: {
                Value vb = stack_manager.pop();
                Value va = stack_manager.pop();
                std::string sa = (va.type == ValueType::OBJECT && va.current_value.object)
                                     ? dynamic_cast<ObjString *>(va.current_value.object)->value
                                     : va.toString();
                std::string sb = (vb.type == ValueType::OBJECT && vb.current_value.object)
                                     ? dynamic_cast<ObjString *>(vb.current_value.object)->value
                                     : vb.toString();
                ObjString *sNew = allocator.allocate_string(sa + sb);
                stack_manager.push(Value::createOBJECT(sNew));
                break;
            }
            case OP_STRING_LENGTH: {
                Value s = stack_manager.pop();
                if (s.type == ValueType::OBJECT && s.current_value.object) {
                    auto *os = dynamic_cast<ObjString *>(s.current_value.object);
                    if (os) {
                        stack_manager.push(Value::createINT(static_cast<int64_t>(os->value.size())));
                        break;
                    }
                }
                stack_manager.push(Value::createINT(0));
                break;
            }
            case OP_STRING_SUBSTR: {
                uint32_t start = frame.read_u32();
                uint32_t len = frame.read_u32();
                Value s = stack_manager.pop();
                if (s.type == ValueType::OBJECT && s.current_value.object) {
                    auto *os = dynamic_cast<ObjString *>(s.current_value.object);
                    if (os) {
                        uint32_t st = start;
                        if (st > os->value.size()) st = os->value.size();
                        uint32_t l = std::min<uint32_t>(len, static_cast<uint32_t>(os->value.size()) - st);
                        ObjString *out = allocator.allocate_string(os->value.substr(st, l));
                        stack_manager.push(Value::createOBJECT(out));
                        break;
                    }
                }
                stack_manager.push(Value::createOBJECT(allocator.allocate_string(std::string(""))));
                break;
            }
            case OP_STRING_EQ: {
                Value b = stack_manager.pop();
                Value a = stack_manager.pop();
                std::string sa = (a.type == ValueType::OBJECT && a.current_value.object)
                                     ? dynamic_cast<ObjString *>(a.current_value.object)->value
                                     : a.toString();
                std::string sb = (b.type == ValueType::OBJECT && b.current_value.object)
                                     ? dynamic_cast<ObjString *>(b.current_value.object)->value
                                     : b.toString();
                stack_manager.push(Value::createBOOL(sa == sb));
                break;
            }
            case OP_STRING_GET_CHAR: {
                Value idxv = stack_manager.pop();
                Value s = stack_manager.pop();
                int idx = (idxv.type == ValueType::INT) ? static_cast<int>(idxv.current_value.i) : 0;
                if (s.type == ValueType::OBJECT && s.current_value.object) {
                    auto os = dynamic_cast<ObjString *>(s.current_value.object);
                    if (os && idx >= 0 && idx < static_cast<int>(os->value.size())) {
                        std::string ch(1, os->value[idx]);
                        ObjString *out = allocator.allocate_string(ch);
                        stack_manager.push(Value::createOBJECT(out));
                        break;
                    }
                }
                stack_manager.push(Value::createOBJECT(allocator.allocate_string(std::string(""))));
                break;
            }

            // Globals
            case OP_LOAD_GLOBAL: {
                uint32_t nameIdx = frame.read_u32();
                if (nameIdx >= global_constants.size()) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                auto *on = dynamic_cast<ObjString *>(global_constants[nameIdx].current_value.object);
                if (!on) {
                    stack_manager.push(Value::createNIL());
                    break;
                }
                auto it = globals.find(on->value);
                if (it == globals.end()) stack_manager.push(Value::createNIL());
                else stack_manager.push(it->second);
                break;
            }
            case OP_STORE_GLOBAL: {
                uint32_t nameIdx = frame.read_u32();
                Value val = stack_manager.pop();
                if (nameIdx >= global_constants.size()) break;
                auto on = dynamic_cast<ObjString *>(global_constants[nameIdx].current_value.object);
                if (!on) break;
                globals[on->value] = val;
                break;
            }

            case OP_NEW_ARRAY: {
                ObjArray *arr = allocator.allocate_array();
                stack_manager.push(Value::createOBJECT(arr));
                break;
            }
            case OP_NEW_MAP: {
                ObjMap *map = allocator.allocate_map();
                stack_manager.push(Value::createOBJECT(map));
                break;
            }

            default:
                std::cerr << "Unimplemented opcode: " << static_cast<int>(op) << "\n";
                return;
        }
    }
}

uint32_t VM::add_global_string_constant(const std::string &s) {
    ObjString *os = allocator.allocate_string(s);
    const auto idx = static_cast<uint32_t>(global_constants.size());
    global_constants.push_back(Value::createOBJECT(os));
    return idx;
}
