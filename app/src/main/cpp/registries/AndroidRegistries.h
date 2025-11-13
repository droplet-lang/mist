//
// Created by NITRO on 11/13/2025.
//

#ifndef MIST_ANDROIDREGISTRIES_H
#define MIST_ANDROIDREGISTRIES_H

#include "../droplet/src/compiler/TypeChecker.h"
#include "../droplet/src/native/NativeRegisteries.h"

inline void initAndroidBuiltins() {
    registerNative({"android_native_toast", Type::String(), {}});
}


#endif //MIST_ANDROIDREGISTRIES_H
