//
// Created by NITRO on 11/13/2025.
//

#ifndef DROPLET_ANDROIDNATIVE_H
#define DROPLET_ANDROIDNATIVE_H

#if defined(__ANDROID__)
#include <cstdint>
#include "../droplet/src/vm/VM.h"

void android_native_toast(VM& vm, const uint8_t argc);

inline void register_android_native_functions(VM& vm) {
    vm.register_native("android_native_toast", android_native_toast);
}
#endif

#endif //DROPLET_ANDROIDNATIVE_H