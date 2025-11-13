//
// Created by NITRO on 11/13/2025.
//

#ifndef MIST_ANDROIDREGISTRIES_H
#define MIST_ANDROIDREGISTRIES_H

#include "../droplet/src/compiler/TypeChecker.h"
#include "../droplet/src/native/NativeRegisteries.h"

inline void initAndroidBuiltins() {
    registerNative({"android_create_button", Type::String(), {}});
    registerNative({"android_native_toast", Type::String(), {}});

    registerNative({"android_create_textview", Type::Int(), {}}); // returns id
    registerNative({"android_create_imageview", Type::Int(), {}});
    registerNative({"android_create_linearlayout", Type::Int(), {}});
    registerNative({"android_add_view_to_parent", Type::Null(), {}});
    registerNative({"android_set_view_text", Type::Null(), {}});
    registerNative({"android_set_view_image", Type::Null(), {}});
    registerNative({"android_set_view_visibility", Type::Null(), {}});
}


#endif //MIST_ANDROIDREGISTRIES_H
