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
#ifndef DROPLET_LOADER_H
#define DROPLET_LOADER_H
#include <cstdint>
#include <string>
#include <vector>

#include "VM.h"

/*
 * The dbc file will be the one containing information about constants, functions and code
 * It's format:
 *
 *     |  HEADER "DLBC" (4)           |    VERSION (1)    |
 *     |  constant_count (u32t)       |    [...constants] |
 *     |  function_count (u32t)       |    [...fn header] |
 *     |  code_size (u32t)            |    [...byte code] |
 *
 * Here:
 *
 * [...constants]
 *      TYPE (u8t) == which indicate size of this constant in consequitive byte
 *          1 = i32t
 *          2 = double
 *          3 = u32t len; bytes[len] (for string)
 *          4 = no data (NIL0
 *          5 = u8t(0/1 - BOOL)
 *
 * [...fn header]
 *      u32t nameIndex
 *      u32t start
 *      u32t size
 *      u8t  argCount
 *      u8t  localCount
 */

struct Loader {
    static uint32_t read_u32(const std::vector<uint8_t> &buf, size_t &off);
    static uint16_t read_u16(const std::vector<uint8_t> &buf, size_t &off);
    static uint8_t read_u8(const std::vector<uint8_t> &buf, size_t &off);
    static double read_double(const std::vector<uint8_t> &buf, size_t &off);
    static int32_t read_i32(const std::vector<uint8_t> &buf, size_t &off);
    bool load_dbc_file(const std::string &path, VM &vm);
};


#endif //DROPLET_LOADER_H