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
 *  File: DBC_helper.cpp
 *  Created: 11/8/2025
 * ============================================================
 */
#include "DBC_helper.h"
#include <cstring>
#include <fstream>
#include <iostream>

// DBCBuilder implementations
uint32_t DBCBuilder::addConstInt(const int32_t value) {
    const uint32_t idx = static_cast<uint32_t>(constants.size());
    constants.push_back({1, std::vector<uint8_t>()});
    write_i32(constants.back().data, value);
    return idx;
}

uint32_t DBCBuilder::addConstDouble(const double value) {
    const uint32_t idx = static_cast<uint32_t>(constants.size());
    constants.push_back({2, std::vector<uint8_t>()});
    write_double(constants.back().data, value);
    return idx;
}

uint32_t DBCBuilder::addConstString(const std::string& value) {
    const uint32_t idx = static_cast<uint32_t>(constants.size());
    constants.push_back({3, std::vector<uint8_t>()});
    write_u32(constants.back().data, static_cast<uint32_t>(value.size()));
    for (const char c : value) {
        constants.back().data.push_back(static_cast<uint8_t>(c));
    }
    return idx;
}

uint32_t DBCBuilder::addConstNil() {
    const uint32_t idx = static_cast<uint32_t>(constants.size());
    constants.push_back({4, std::vector<uint8_t>()});
    return idx;
}

uint32_t DBCBuilder::addConstBool(const bool value) {
    const uint32_t idx = static_cast<uint32_t>(constants.size());
    constants.push_back({5, std::vector<uint8_t>()});
    constants.back().data.push_back(value ? 1 : 0);
    return idx;
}

DBCBuilder::FunctionBuilder& DBCBuilder::addFunction(const std::string& name) {
    functions.emplace_back();
    functions.back().name = name;
    return functions.back();
}

bool DBCBuilder::writeToFile(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open " << path << " for writing\n";
        return false;
    }

    // FIRST: Add all function names to constants (before writing constants section)
    std::vector<uint32_t> functionNameIndices;

    for (auto& fn : functions) {
        uint32_t nameIdx = findOrAddStringConstant(fn.name);
        functionNameIndices.push_back(nameIdx);
    }

    // Header: "DLBC"
    file.write("DLBC", 4);

    // Version: 1
    constexpr uint8_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), 1);

    // Constants section
    const uint32_t constCount = static_cast<uint32_t>(constants.size());
    write_to_file(file, constCount);

    for (auto& c : constants) {
        file.write(reinterpret_cast<const char*>(&c.type), 1);
        file.write(reinterpret_cast<const char*>(c.data.data()), c.data.size());
    }

    // Build unified code section and function headers
    std::vector<uint8_t> unifiedCode;
    struct FnHeader {
        uint32_t nameIdx;
        uint32_t start;
        uint32_t size;
        uint8_t argCount;
        uint8_t localCount;
    };
    std::vector<FnHeader> headers;

    for (auto& fn : functions) {
        // Find or add function name to constants
        const uint32_t nameIdx = findOrAddStringConstant(fn.name);

        FnHeader h;
        h.nameIdx = nameIdx;
        h.start = static_cast<uint32_t>(unifiedCode.size());
        h.size = static_cast<uint32_t>(fn.code.size());
        h.argCount = fn.argCount;
        h.localCount = fn.localCount;
        headers.push_back(h);

        // Append code
        unifiedCode.insert(unifiedCode.end(), fn.code.begin(), fn.code.end());
    }

    // Write function count
    const uint32_t fnCount = static_cast<uint32_t>(functions.size());
    write_to_file(file, fnCount);

    // Write function headers
    for (auto& h : headers) {
        write_to_file(file, h.nameIdx);
        write_to_file(file, h.start);
        write_to_file(file, h.size);
        file.write(reinterpret_cast<const char*>(&h.argCount), 1);
        file.write(reinterpret_cast<const char*>(&h.localCount), 1);
    }

    // Write code section
    const uint32_t codeSize = static_cast<uint32_t>(unifiedCode.size());
    write_to_file(file, codeSize);
    file.write(reinterpret_cast<const char*>(unifiedCode.data()), codeSize);

    file.close();
    std::cout << "DBC file written: " << path << "\n";
    std::cout << "  Constants: " << constCount << "\n";
    std::cout << "  Functions: " << fnCount << "\n";
    std::cout << "  Code size: " << codeSize << " bytes\n";
    return true;
}

uint32_t DBCBuilder::findOrAddStringConstant(const std::string& str) {
    // Check if string already exists in constants
    for (size_t i = 0; i < constants.size(); ++i) {
        if (constants[i].type == 3) { // String type
            // Extract string from data
            uint32_t len;
            memcpy(&len, constants[i].data.data(), sizeof(uint32_t));
            std::string existing(reinterpret_cast<const char*>(constants[i].data.data() + 4), len);
            if (existing == str) {
                return static_cast<uint32_t>(i);
            }
        }
    }
    // Not found, add new
    return addConstString(str);
}

void DBCBuilder::write_i32(std::vector<uint8_t>& buf, const int32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void DBCBuilder::write_u32(std::vector<uint8_t>& buf, const uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void DBCBuilder::write_double(std::vector<uint8_t>& buf, const double val) {
    const auto bytes = reinterpret_cast<const uint8_t*>(&val);
    for (size_t i = 0; i < sizeof(double); ++i) {
        buf.push_back(bytes[i]);
    }
}

void DBCBuilder::write_to_file(std::ofstream& file, const uint32_t val) {
    file.write(reinterpret_cast<const char*>(&val), sizeof(val));
}

// FunctionBuilder implementations
DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::setName(const std::string& n) {
    name = n;
    return *this;
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::setArgCount(const uint8_t count) {
    argCount = count;
    return *this;
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::setLocalCount(const uint8_t count) {
    localCount = count;
    return *this;
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::emit(const Op op) {
    code.push_back(static_cast<uint8_t>(op));
    return *this;
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::emitU8(const uint8_t val) {
    code.push_back(val);
    return *this;
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::emitU16(const uint16_t val) {
    code.push_back(static_cast<uint8_t>(val & 0xFF));
    code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    return *this;
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::emitU32(const uint32_t val) {
    code.push_back(static_cast<uint8_t>(val & 0xFF));
    code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    code.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    code.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    return *this;
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::pushConst(const uint32_t constIdx) {
    return emit(OP_PUSH_CONST).emitU32(constIdx);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::loadLocal(const uint8_t slot) {
    return emit(OP_LOAD_LOCAL).emitU8(slot);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::storeLocal(const uint8_t slot) {
    return emit(OP_STORE_LOCAL).emitU8(slot);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::loadGlobal(const uint32_t nameIdx) {
    return emit(OP_LOAD_GLOBAL).emitU32(nameIdx);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::storeGlobal(const uint32_t nameIdx) {
    return emit(OP_STORE_GLOBAL).emitU32(nameIdx);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::call(const uint32_t fnIdx, const uint8_t argc) {
    return emit(OP_CALL).emitU32(fnIdx).emitU8(argc);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::ret(const uint8_t retCount) {
    return emit(OP_RETURN).emitU8(retCount);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::jump(const uint32_t target) {
    return emit(OP_JUMP).emitU32(target);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::jumpIfFalse(const uint32_t target) {
    return emit(OP_JUMP_IF_FALSE).emitU32(target);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::jumpIfTrue(const uint32_t target) {
    return emit(OP_JUMP_IF_TRUE).emitU32(target);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::newArray() {
    return emit(OP_NEW_ARRAY);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::newMap() {
    return emit(OP_NEW_MAP);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::newObject(const uint32_t classNameIdx) {
    return emit(OP_NEW_OBJECT).emitU32(classNameIdx);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::getField(const uint32_t fieldNameIdx) {
    return emit(OP_GET_FIELD).emitU32(fieldNameIdx);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::setField(const uint32_t fieldNameIdx) {
    return emit(OP_SET_FIELD).emitU32(fieldNameIdx);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::arrayGet() {
    return emit(OP_ARRAY_GET);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::arraySet() {
    return emit(OP_ARRAY_SET);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::mapGet() {
    return emit(OP_MAP_GET);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::mapSet() {
    return emit(OP_MAP_SET);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::callFFI(const uint32_t libIdx, const uint32_t symIdx, const uint8_t argc, const uint32_t sigIdx) {
    return emit(OP_CALL_FFI)
        .emitU32(libIdx)
        .emitU32(symIdx)
        .emitU8(argc)
        .emitU32(sigIdx);
}

DBCBuilder::FunctionBuilder& DBCBuilder::FunctionBuilder::isInstance(uint32_t typeNameIdx) {
    return emit(OP_IS_INSTANCE).emitU32(typeNameIdx);
}

uint32_t DBCBuilder::FunctionBuilder::currentPos() const {
    return static_cast<uint32_t>(code.size());
}