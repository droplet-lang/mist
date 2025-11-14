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
void android_set_view_property(VM& vm, const uint8_t argc);
void android_create_scrollview(VM& vm, const uint8_t argc);
void android_create_cardview(VM& vm, const uint8_t argc);
void android_create_recyclerview(VM& vm, const uint8_t argc);
void android_recyclerview_add_item(VM& vm, const uint8_t argc);
void android_recyclerview_clear(VM& vm, const uint8_t argc);
void android_set_view_background_color(VM& vm, const uint8_t argc);
void android_set_view_padding(VM& vm, const uint8_t argc);
void android_set_view_size(VM& vm, const uint8_t argc);

// NEW: Toolbar and Navigation
void android_set_toolbar_title(VM& vm, const uint8_t argc);
void android_create_screen(VM& vm, const uint8_t argc);
void android_navigate_to_screen(VM& vm, const uint8_t argc);
void android_navigate_back(VM& vm, const uint8_t argc);
void android_set_back_button_visible(VM& vm, const uint8_t argc);

inline void register_android_native_functions(VM& vm) {
    vm.register_native("android_native_toast", android_native_toast);
    vm.register_native("android_create_button", android_create_button);

    // existing views
    vm.register_native("android_create_textview", android_create_textview);
    vm.register_native("android_create_imageview", android_create_imageview);
    vm.register_native("android_create_linearlayout", android_create_linearlayout);
    vm.register_native("android_add_view_to_parent", android_add_view_to_parent);

    vm.register_native("android_set_view_text", android_set_view_text);
    vm.register_native("android_set_view_image", android_set_view_image);
    vm.register_native("android_set_view_visibility", android_set_view_visibility);
    vm.register_native("android_set_view_property", android_set_view_property);
    vm.register_native("android_create_scrollview", android_create_scrollview);
    vm.register_native("android_create_cardview", android_create_cardview);
    vm.register_native("android_create_recyclerview", android_create_recyclerview);
    vm.register_native("android_recyclerview_add_item", android_recyclerview_add_item);
    vm.register_native("android_recyclerview_clear", android_recyclerview_clear);
    vm.register_native("android_set_view_background_color", android_set_view_background_color);
    vm.register_native("android_set_view_padding", android_set_view_padding);
    vm.register_native("android_set_view_size", android_set_view_size);

    // NEW: Toolbar and Navigation
    vm.register_native("android_set_toolbar_title", android_set_toolbar_title);
    vm.register_native("android_create_screen", android_create_screen);
    vm.register_native("android_navigate_to_screen", android_navigate_to_screen);
    vm.register_native("android_navigate_back", android_navigate_back);
    vm.register_native("android_set_back_button_visible", android_set_back_button_visible);
}
#endif

#endif //DROPLET_ANDROIDNATIVE_H