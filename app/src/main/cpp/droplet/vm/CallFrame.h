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
#ifndef DROPLET_CALLFRAME_H
#define DROPLET_CALLFRAME_H
#include <cstdint>

#include "Function.h"


struct CallFrame {
    Function *function = nullptr;
    uint32_t ip = 0; // which line we are in, in the function code
    uint32_t localStartsAt = 0; // index in VM stack, where this frame locals starts


    uint8_t read_u8();
    uint8_t read_u16();
    uint8_t read_u32();
};



#endif //DROPLET_CALLFRAME_H