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
 *  File: DBC_helper.h
 *  Created: 11/8/2025
 * ============================================================
 */
#ifndef DROPLET_DBC_HELPER_H
#define DROPLET_DBC_HELPER_H

#include <string>
#include <vector>
#include <cstdint>
#include "defines.h"

class DBCBuilder {
public:
    DBCBuilder() = default;

    // Add constants to the constant pool
    uint32_t addConstInt(int32_t value);
    uint32_t addConstDouble(double value);
    uint32_t addConstString(const std::string& value);
    uint32_t addConstNil();
    uint32_t addConstBool(bool value);

    struct FunctionBuilder {
        std::string name;
        uint8_t argCount = 0;
        uint8_t localCount = 0;
        std::vector<uint8_t> code;

        FunctionBuilder& setName(const std::string& n);
        FunctionBuilder& setArgCount(uint8_t count);
        FunctionBuilder& setLocalCount(uint8_t count);

        // Emit opcodes
        FunctionBuilder& emit(Op op);
        FunctionBuilder& emitU8(uint8_t val);
        FunctionBuilder& emitU16(uint16_t val);
        FunctionBuilder& emitU32(uint32_t val);

        FunctionBuilder& pushConst(uint32_t constIdx);
        FunctionBuilder& loadLocal(uint8_t slot);
        FunctionBuilder& storeLocal(uint8_t slot);
        FunctionBuilder& loadGlobal(uint32_t nameIdx);
        FunctionBuilder& storeGlobal(uint32_t nameIdx);
        FunctionBuilder& call(uint32_t fnIdx, uint8_t argc);
        FunctionBuilder& ret(uint8_t retCount = 1);
        FunctionBuilder& jump(uint32_t target);
        FunctionBuilder& jumpIfFalse(uint32_t target);
        FunctionBuilder& jumpIfTrue(uint32_t target);
        FunctionBuilder& newArray();
        FunctionBuilder& newMap();
        FunctionBuilder& newObject(uint32_t classNameIdx);
        FunctionBuilder& getField(uint32_t fieldNameIdx);
        FunctionBuilder& setField(uint32_t fieldNameIdx);
        FunctionBuilder& arrayGet();
        FunctionBuilder& arraySet();
        FunctionBuilder& mapGet();
        FunctionBuilder& mapSet();
        FunctionBuilder& callFFI(uint32_t libIdx, uint32_t symIdx, uint8_t argc, uint32_t sigIdx);
        FunctionBuilder& isInstance(uint32_t typeNameIdx);

        // Get current code position (useful for jump targets)
        uint32_t currentPos() const;
    };

    FunctionBuilder& addFunction(const std::string& name);

    // Write the DBC file
    bool writeToFile(const std::string& path);

    std::vector<FunctionBuilder> functions;

private:
    struct Constant {
        uint8_t type;
        std::vector<uint8_t> data;
    };

    std::vector<Constant> constants;

    uint32_t findOrAddStringConstant(const std::string& str);

    static void write_i32(std::vector<uint8_t>& buf, int32_t val);
    static void write_u32(std::vector<uint8_t>& buf, uint32_t val);
    static void write_double(std::vector<uint8_t>& buf, double val);
    static void write_to_file(std::ofstream& file, uint32_t val);
};

#endif //DROPLET_DBC_HELPER_H