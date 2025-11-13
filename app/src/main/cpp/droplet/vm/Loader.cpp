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
 *  File: Loader
 *  Created: 11/9/2025
 * ============================================================
 */
#include "Loader.h"

#include <fstream>
#include <iostream>

#include "Object.h"
#include "Value.h"

uint32_t Loader::read_u32(const std::vector<uint8_t> &buf, size_t &off) {
    const uint32_t v = static_cast<uint32_t>(buf[off]) | (static_cast<uint32_t>(buf[off + 1]) << 8) | (
                           static_cast<uint32_t>(buf[off + 2]) << 16) | (static_cast<uint32_t>(buf[off + 3]) << 24);
    off += 4;
    return v;
}

uint16_t Loader::read_u16(const std::vector<uint8_t> &buf, size_t &off) {
    const uint16_t v = static_cast<uint16_t>(buf[off]) | (static_cast<uint16_t>(buf[off + 1]) << 8);
    off += 2;
    return v;
}

uint8_t Loader::read_u8(const std::vector<uint8_t> &buf, size_t &off) {
    const uint8_t v = buf[off];
    off += 1;
    return v;
}

double Loader::read_double(const std::vector<uint8_t> &buf, size_t &off) {
    double x;
    memcpy(&x, &buf[off], sizeof(double));
    off += sizeof(double);
    return x;
}

int32_t Loader::read_i32(const std::vector<uint8_t> &buf, size_t &off) {
    int32_t v;
    memcpy(&v, &buf[off], sizeof(int32_t));
    off += 4;
    return v;
}

bool Loader::load_dbc_file(const std::string &path, VM &vm) {
    std::ifstream f(path, std::ios::binary);

    if (!f.is_open()) {
        std::cerr << "Failed to open " << path << "\n";
        return false;
    }

    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t off = 0;
    if (buf.size() < 5) {
        std::cerr << "Invalid dbc\n";
        return false;
    }

    // check if there is a correct magic string
    std::string magic(reinterpret_cast<char *>(&buf[off]), 4);
    off += 4;

    if (magic != "DLBC") {
        std::cerr << "Bad magic\n";
        return false;
    }

    // test curr version
    uint8_t version = read_u8(buf, off);
    if (version != 1) {
        // Todo (@svpz) hardcoded version doesnot make sense
        std::cerr << "Unsupported version\n";
        return false;
    }

    // constants (global pool)
    uint32_t constCount = read_u32(buf, off);

    // create a list that can hold up constants
    std::vector<Value> constPool;
    constPool.reserve(constCount);

    for (uint32_t i = 0; i < constCount; i++) {
        uint8_t t = read_u8(buf, off);

        if (t == 1) {
            // INT
            int32_t vi = read_i32(buf, off);
            constPool.push_back(Value::createINT(vi));
        } else if (t == 2) {
            // DOUBLE
            double dv = read_double(buf, off);
            constPool.push_back(Value::createDOUBLE(dv));
        } else if (t == 3) {
            // STRING
            uint32_t len = read_u32(buf, off);
            std::string s(reinterpret_cast<char *>(&buf[off]), len);
            off += len;
            ObjString *os = vm.allocator.allocate_string(s);
            constPool.push_back(Value::createOBJECT(os));
        } else if (t == 4) {
            // NIL
            constPool.push_back(Value::createNIL());
        } else if (t == 5) {
            // BOOL
            uint8_t b = read_u8(buf, off);
            constPool.push_back(Value::createBOOL(b != 0));
        } else {
            std::cerr << "Unknown const type " << static_cast<int>(t) << "\n";
            return false;
        }
    }

    // functions
    uint32_t fnCount = read_u32(buf, off);

    struct FnHeader {
        uint32_t nameIndex;
        uint32_t start;
        uint32_t size;
        uint8_t argCount;
        uint8_t localCount;
    };

    std::vector<FnHeader> headers;
    headers.reserve(fnCount);
    for (uint32_t i = 0; i < fnCount; i++) {
        FnHeader h;
        h.nameIndex = read_u32(buf, off);
        h.start = read_u32(buf, off);
        h.size = read_u32(buf, off);
        h.argCount = read_u8(buf, off);
        h.localCount = read_u8(buf, off);
        headers.push_back(h);
    }

    // code section size
    uint32_t codeSize = read_u32(buf, off);

    if (off + codeSize > buf.size()) {
        std::cerr << "Bad code size\n";
        return false;
    }

    std::vector code(buf.begin() + off, buf.begin() + off + codeSize);
    off += codeSize;

    // create Function entries, each referring to its sub-slice of code and constants.
    for (uint32_t i = 0; i < fnCount; i++) {
        FnHeader &h = headers[i];
        // name from constPool[nameIndex]
        if (h.nameIndex >= constPool.size() || constPool[h.nameIndex].type != ValueType::OBJECT) {
            std::cerr << "Invalid name index for function header\n";
            return false;
        }

        auto on = dynamic_cast<ObjString *>(constPool[h.nameIndex].current_value.object);
        if (!on) {
            std::cerr << "Function name not string\n";
            return false;
        }

        std::string fname = on->value;
        auto fn = std::make_unique<Function>();

        fn->name = fname;
        fn->argCount = h.argCount;
        fn->localCount = h.localCount;

        // For simplicity: all function constants will be empty here (we use global constants only)
        // copy the code slice
        if (h.start + h.size > code.size()) {
            std::cerr << "Function code out of bounds\n";
            return false;
        }

        fn->code.assign(code.begin() + h.start, code.begin() + h.start + h.size);

        // add to function table
        uint32_t idx = static_cast<uint32_t>(vm.functions.size());
        vm.function_index_by_name[fn->name] = idx;
        vm.functions.push_back(std::move(fn));
    }

    // for now we store global constants (strings) as globalConstants
    for (auto &c: constPool) vm.global_constants.push_back(c);
    return true;
}