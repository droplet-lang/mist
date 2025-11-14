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

    // Layout views
    registerNative({"android_create_linearlayout", Type::Int(), {}});
    registerNative({"android_create_scrollview", Type::Int(), {}});
    registerNative({"android_create_cardview", Type::Int(), {}});
    registerNative({"android_create_recyclerview", Type::Int(), {}});

    // Basic views
    registerNative({"android_create_textview", Type::Int(), {}});
    registerNative({"android_create_imageview", Type::Int(), {}});

    // View manipulation
    registerNative({"android_add_view_to_parent", Type::Null(), {}});
    registerNative({"android_set_view_text", Type::Null(), {}});
    registerNative({"android_set_view_image", Type::Null(), {}});
    registerNative({"android_set_view_visibility", Type::Null(), {}});
    registerNative({"android_set_view_background_color", Type::Null(), {}});
    registerNative({"android_set_view_padding", Type::Null(), {}});
    registerNative({"android_set_view_size", Type::Null(), {}});

    // RecyclerView specific
    registerNative({"android_recyclerview_add_item", Type::Null(), {}});
    registerNative({"android_recyclerview_clear", Type::Null(), {}});
}


#endif //MIST_ANDROIDREGISTRIES_H
