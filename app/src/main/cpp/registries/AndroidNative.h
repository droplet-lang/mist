#ifndef DROPLET_ANDROIDNATIVE_H
#define DROPLET_ANDROIDNATIVE_H

#if defined(__ANDROID__)
#include <cstdint>
#include "../droplet/src/vm/VM.h"

void android_set_vm_instance(VM* vm);

// existing
void android_native_toast(VM& vm, const uint8_t argc);
void android_create_button(VM& vm, const uint8_t argc);

// new functions - create/modify views
void android_create_textview(VM& vm, const uint8_t argc);
void android_create_imageview(VM& vm, const uint8_t argc);
void android_create_linearlayout(VM& vm, const uint8_t argc);
void android_add_view_to_parent(VM& vm, const uint8_t argc);

// programmatic updates
void android_set_view_text(VM& vm, const uint8_t argc);
void android_set_view_image(VM& vm, const uint8_t argc);
void android_set_view_visibility(VM& vm, const uint8_t argc);
void android_set_view_property(VM& vm, const uint8_t argc); // generic property setter

inline void register_android_native_functions(VM& vm) {
    vm.register_native("android_native_toast", android_native_toast);
    vm.register_native("android_create_button", android_create_button);

    // new
    vm.register_native("android_create_textview", android_create_textview);
    vm.register_native("android_create_imageview", android_create_imageview);
    vm.register_native("android_create_linearlayout", android_create_linearlayout);
    vm.register_native("android_add_view_to_parent", android_add_view_to_parent);

    vm.register_native("android_set_view_text", android_set_view_text);
    vm.register_native("android_set_view_image", android_set_view_image);
    vm.register_native("android_set_view_visibility", android_set_view_visibility);
    vm.register_native("android_set_view_property", android_set_view_property);
}
#endif

#endif //DROPLET_ANDROIDNATIVE_H
