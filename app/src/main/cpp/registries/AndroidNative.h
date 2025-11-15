#ifndef DROPLET_ANDROIDNATIVE_H
#define DROPLET_ANDROIDNATIVE_H

#if defined(__ANDROID__)
#include <cstdint>
#include "../droplet/src/vm/VM.h"

void android_set_vm_instance(VM* vm);

// existing
void android_native_toast(VM& vm, const uint8_t argc);
void android_create_button(VM& vm, const uint8_t argc);

// view creation
void android_create_textview(VM& vm, const uint8_t argc);
void android_create_imageview(VM& vm, const uint8_t argc);
void android_create_linearlayout(VM& vm, const uint8_t argc);
void android_add_view_to_parent(VM& vm, const uint8_t argc);

// NEW: Text Input
void android_create_edittext(VM& vm, const uint8_t argc);
void android_get_edittext_value(VM& vm, const uint8_t argc);
void android_set_edittext_hint(VM& vm, const uint8_t argc);
void android_set_edittext_input_type(VM& vm, const uint8_t argc);

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

// NEW: Styling Functions
void android_set_text_size(VM& vm, const uint8_t argc);
void android_set_text_color(VM& vm, const uint8_t argc);
void android_set_text_style(VM& vm, const uint8_t argc);
void android_set_view_margin(VM& vm, const uint8_t argc);
void android_set_view_gravity(VM& vm, const uint8_t argc);
void android_set_view_elevation(VM& vm, const uint8_t argc);
void android_set_view_corner_radius(VM& vm, const uint8_t argc);
void android_set_view_border(VM& vm, const uint8_t argc);

// Toolbar and Navigation
void android_set_toolbar_title(VM& vm, const uint8_t argc);
void android_create_screen(VM& vm, const uint8_t argc);
void android_navigate_to_screen(VM& vm, const uint8_t argc);
void android_navigate_back(VM& vm, const uint8_t argc);
void android_set_back_button_visible(VM& vm, const uint8_t argc);
void android_clear_screen(VM& vm, const uint8_t argc);

// HTTP Functions
void android_http_get(VM& vm, const uint8_t argc);
void android_http_post(VM& vm, const uint8_t argc);
void android_http_put(VM& vm, const uint8_t argc);
void android_http_delete(VM& vm, const uint8_t argc);

inline void register_android_native_functions(VM& vm) {
    vm.register_native("android_native_toast", android_native_toast);
    vm.register_native("android_create_button", android_create_button);

    // existing views
    vm.register_native("android_create_textview", android_create_textview);
    vm.register_native("android_create_imageview", android_create_imageview);
    vm.register_native("android_create_linearlayout", android_create_linearlayout);
    vm.register_native("android_add_view_to_parent", android_add_view_to_parent);

    // NEW: Text Input
    vm.register_native("android_create_edittext", android_create_edittext);
    vm.register_native("android_get_edittext_value", android_get_edittext_value);
    vm.register_native("android_set_edittext_hint", android_set_edittext_hint);
    vm.register_native("android_set_edittext_input_type", android_set_edittext_input_type);

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

    // Styling Functions
    vm.register_native("android_set_text_size", android_set_text_size);
    vm.register_native("android_set_text_color", android_set_text_color);
    vm.register_native("android_set_text_style", android_set_text_style);
    vm.register_native("android_set_view_margin", android_set_view_margin);
    vm.register_native("android_set_view_gravity", android_set_view_gravity);
    vm.register_native("android_set_view_elevation", android_set_view_elevation);
    vm.register_native("android_set_view_corner_radius", android_set_view_corner_radius);
    vm.register_native("android_set_view_border", android_set_view_border);

    // Toolbar and Navigation
    vm.register_native("android_set_toolbar_title", android_set_toolbar_title);
    vm.register_native("android_create_screen", android_create_screen);
    vm.register_native("android_navigate_to_screen", android_navigate_to_screen);
    vm.register_native("android_navigate_back", android_navigate_back);
    vm.register_native("android_set_back_button_visible", android_set_back_button_visible);
    vm.register_native("android_clear_screen", android_clear_screen);

    // HTTP Functions
    vm.register_native("android_http_get", android_http_get);
    vm.register_native("android_http_post", android_http_post);
    vm.register_native("android_http_put", android_http_put);
    vm.register_native("android_http_delete", android_http_delete);
}
#endif

#endif //DROPLET_ANDROIDNATIVE_H