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
 *  File: CallFrame
 *  Created: 11/9/2025
 * ============================================================
 */
#include "CallFrame.h"
#include "Function.h"

uint8_t CallFrame::read_u8() {
    const uint8_t v = this->function->code[this->ip++];
    return v;
}

uint8_t CallFrame::read_u16() {
    const auto &c = this->function->code;
    const uint16_t v = static_cast<uint16_t>(c[this->ip]) | (static_cast<uint16_t>(c[this->ip + 1]) << 8);
    this->ip += 2;
    return v;
}

uint8_t CallFrame::read_u32() {
    const auto &c = this->function->code;
    const uint32_t v = static_cast<uint32_t>(c[this->ip]) | (static_cast<uint32_t>(c[this->ip + 1]) << 8) | (
                           static_cast<uint32_t>(c[this->ip + 2]) << 16) | (
                           static_cast<uint32_t>(c[this->ip + 3]) << 24);
    this->ip += 4;
    return v;
}