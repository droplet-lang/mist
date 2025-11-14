#include "AndroidNative.h"

#if defined(__ANDROID__)

#include <android/log.h>
#include <jni.h>
#include <unordered_map>
#include "../droplet/src/vm/VM.h"

#define LOG_TAG "DropletVM"

extern "C" {
JavaVM* droplet_java_vm = nullptr;
jobject droplet_activity = nullptr;
static int g_next_view_id = 1000; // start from 1000 to avoid collision

JNIEXPORT void JNICALL
Java_com_mist_example_MainActivity_registerVM(JNIEnv* env, jobject thiz) {
    env->GetJavaVM(&droplet_java_vm);
    droplet_activity = env->NewGlobalRef(thiz);
}
}

// Global VM reference and callback storage
static VM* g_vm_instance = nullptr;

void push_int_to_vm_stack(VM& vm, int n) {
    // Replace with your actual Value creation for integers
    vm.stack_manager.push(Value::createINT(n));
}

struct CallbackInfo {
    Value callback;  // Store the actual callback object (ObjBoundMethod or ObjFunction)
    Object* gcRoot;     // Keep a direct pointer to prevent GC
};

static std::unordered_map<int, CallbackInfo> g_callbacks;
static int g_next_callback_id = 1;

// Global list of GC roots for callbacks
static std::vector<Object*> g_callback_gc_roots;

void android_set_vm_instance(VM* vm) {
    g_vm_instance = vm;
}

void android_native_toast(VM& vm, const uint8_t argc) {
    if (argc == 0) {
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value msg = vm.stack_manager.pop();
    for (int i = 1; i < argc; i++) {
        vm.stack_manager.pop();
    }

    std::string str = msg.toString();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "showToast", "(Ljava/lang/String;)V");

    jstring jmsg = env->NewStringUTF(str.c_str());
    env->CallVoidMethod(droplet_activity, method, jmsg);
    env->DeleteLocalRef(jmsg);

    vm.stack_manager.push(Value::createNIL());
}

void android_create_button(VM& vm, const uint8_t argc) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "create_button called with argc: %d", argc);

    if(argc < 2) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "create_button requires at least 2 args (got %d)", argc);
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    // Pop arguments in reverse order (last pushed = first popped)
    int parentId = -1;
    if (argc >= 3) {
        Value parent = vm.stack_manager.pop();
        parentId = (parent.type == ValueType::INT) ? parent.current_value.i : -1;
    }

    Value callback = vm.stack_manager.pop();
    Value title = vm.stack_manager.pop();

    // Pop any extra arguments
    for (int i = 3; i < argc; i++) {
        vm.stack_manager.pop();
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Button: %s, Callback type: %d, ParentId: %d",
                        title.toString().c_str(), static_cast<int>(callback.type), parentId);

    // Store callback info
    int callbackId = g_next_callback_id++;
    CallbackInfo info;
    info.callback = callback;

    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        info.gcRoot = callback.current_value.object;
        g_callback_gc_roots.push_back(callback.current_value.object);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Stored GC root at %p", callback.current_value.object);
    } else {
        info.gcRoot = nullptr;
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Warning: callback is not an object!");
    }

    g_callbacks[callbackId] = info;

    // Don't generate a buttonId - let Java handle it
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Created callback ID: %d", callbackId);

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    // Use the original 3-parameter signature
    jmethodID method = env->GetMethodID(cls, "createButton", "(Ljava/lang/String;II)V");

    jstring jtitle = env->NewStringUTF(title.toString().c_str());
    env->CallVoidMethod(droplet_activity, method, jtitle, callbackId, parentId);
    env->DeleteLocalRef(jtitle);

    // Return NIL since we don't need the button ID in Droplet
    vm.stack_manager.push(Value::createNIL());
}

// Called from Java when button is clicked
extern "C"
JNIEXPORT void JNICALL
Java_com_mist_example_MainActivity_onButtonClick(JNIEnv* env, jobject thiz, jint callbackId) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Button clicked with callback ID: %d", callbackId);

    if (!g_vm_instance) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "VM instance not set");
        return;
    }

    if (!g_vm_instance->is_ready()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "VM is not ready");
        return;
    }

    auto it = g_callbacks.find(callbackId);
    if (it == g_callbacks.end()) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Callback %d not found", callbackId);
        return;
    }

    CallbackInfo& info = it->second;
    Value callback = info.callback;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Found callback, type: %d", static_cast<int>(callback.type));

    // Log what we're about to execute
    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        auto* boundMethod = dynamic_cast<ObjBoundMethod*>(callback.current_value.object);
        if (boundMethod) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Executing bound method, index: %d",
                                boundMethod->methodIndex);
        }

        auto* fnObj = dynamic_cast<ObjFunction*>(callback.current_value.object);
        if (fnObj) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Executing function, index: %d",
                                fnObj->functionIndex);
        }
    }

    try {
        // Detailed inspection before execution
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Callback value type: %d", static_cast<int>(callback.type));
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Callback object ptr: %p", callback.current_value.object);

        if (callback.type == ValueType::OBJECT && callback.current_value.object) {
            auto* boundMethod = dynamic_cast<ObjBoundMethod*>(callback.current_value.object);
            if (boundMethod) {
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "BoundMethod found! methodIndex=%d", boundMethod->methodIndex);
                __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Functions size=%zu", g_vm_instance->functions.size());
            } else {
                __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Object is not a BoundMethod");

                auto* fnObj = dynamic_cast<ObjFunction*>(callback.current_value.object);
                if (fnObj) {
                    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Function found! functionIndex=%d", fnObj->functionIndex);
                } else {
                    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Object is neither BoundMethod nor Function!");
                }
            }
        } else {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Callback is not an object or is null!");
        }

        // Use the new execute_callback method
        std::vector<Value> args; // No additional arguments for button clicks

        bool success = g_vm_instance->execute_callback(callback, args);

        if (success) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Callback executed successfully");
        } else {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Callback execution failed");
        }

    } catch (const std::exception& e) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Exception in callback: %s", e.what());
    }
}


void android_create_textview(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    // Pop in correct order
    int parentId = -1;
    if (argc >= 2) {
        Value parentVal = vm.stack_manager.pop();
        parentId = (parentVal.type == ValueType::INT) ? parentVal.current_value.i : -1;
    }

    Value textVal = vm.stack_manager.pop();

    // Pop extras
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    std::string text = textVal.toString();
    int viewId = g_next_view_id++;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createTextView", "(Ljava/lang/String;II)V");
    jstring jtext = env->NewStringUTF(text.c_str());
    env->CallVoidMethod(droplet_activity, method, jtext, viewId, parentId);
    env->DeleteLocalRef(jtext);

    push_int_to_vm_stack(vm, viewId);
}


void android_create_imageview(VM& vm, const uint8_t argc) {
    std::string path = "";
    int parentId = -1, width = -1, height = -1;

    // Pop in reverse order (LIFO)
    if (argc >= 4) {
        Value h = vm.stack_manager.pop();
        if (h.type == ValueType::INT) height = h.current_value.i;
    }
    if (argc >= 3) {
        Value w = vm.stack_manager.pop();
        if (w.type == ValueType::INT) width = w.current_value.i;
    }
    if (argc >= 2) {
        Value p = vm.stack_manager.pop();
        if (p.type == ValueType::INT) parentId = p.current_value.i;
    }
    if (argc >= 1) {
        Value pathVal = vm.stack_manager.pop();
        path = pathVal.toString();
    }

    for (int i = 4; i < argc; i++) vm.stack_manager.pop();

    int viewId = g_next_view_id++;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createImageView", "(Ljava/lang/String;IIII)V");

    jstring jpath = env->NewStringUTF(path.c_str());
    env->CallVoidMethod(droplet_activity, method, jpath, viewId, parentId, width, height);
    env->DeleteLocalRef(jpath);

    push_int_to_vm_stack(vm, viewId);
}


void android_create_linearlayout(VM& vm, const uint8_t argc) {
    // args: orientation (0=vertical, 1=horizontal), parentId_opt
    int orientation = 0;
    int parentId = -1;
    if (argc >= 1) {
        Value o = vm.stack_manager.pop();
        orientation = (o.type == ValueType::INT) ? o.current_value.i : 0;
    }
    if (argc >= 2) {
        Value p = vm.stack_manager.pop();
        parentId = (p.type == ValueType::INT) ? p.current_value.i : -1;
    }
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = g_next_view_id++;
    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createLinearLayout", "(III)V"); // path, id, parent
    env->CallVoidMethod(droplet_activity, method, orientation, viewId, parentId);

    push_int_to_vm_stack(vm, viewId);
}

// Add child to parent (native wrapper, but Java can also accept parent at creation)
void android_add_view_to_parent(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value child = vm.stack_manager.pop();
    Value parent = vm.stack_manager.pop();
    int childId = (child.type == ValueType::INT) ? child.current_value.i : -1;
    int parentId = (parent.type == ValueType::INT) ? parent.current_value.i : -1;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "addViewToParent", "(II)V");
    env->CallVoidMethod(droplet_activity, method, parentId, childId);

    vm.stack_manager.push(Value::createNIL());
}

// set text
void android_set_view_text(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }
    Value textVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    std::string text = textVal.toString();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewText", "(ILjava/lang/String;)V");
    jstring jtext = env->NewStringUTF(text.c_str());
    env->CallVoidMethod(droplet_activity, method, viewId, jtext);
    env->DeleteLocalRef(jtext);

    vm.stack_manager.push(Value::createNIL());
}

// set image
void android_set_view_image(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }
    Value pathVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    std::string path = pathVal.toString();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewImage", "(ILjava/lang/String;)V");
    jstring jpath = env->NewStringUTF(path.c_str());
    env->CallVoidMethod(droplet_activity, method, viewId, jpath);
    env->DeleteLocalRef(jpath);

    vm.stack_manager.push(Value::createNIL());
}

// set visibility: 0=VISIBLE, 1=INVISIBLE, 2=GONE
void android_set_view_visibility(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }
    Value visVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int vis = (visVal.type == ValueType::INT) ? visVal.current_value.i : 0;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewVisibility", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, vis);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_view_property(VM& vm, const uint8_t argc) {
    // Optional: generic meta property setter (key, value) for arbitrary properties.
    // Implement similarly: get viewId, propertyName, propertyValue, call Java method to handle reflection.
    vm.stack_manager.push(Value::createNIL());
}

// Create ScrollView
void android_create_scrollview(VM& vm, const uint8_t argc) {
    int parentId = -1;
    if (argc >= 1) {
        Value p = vm.stack_manager.pop();
        parentId = (p.type == ValueType::INT) ? p.current_value.i : -1;
    }
    for (int i = 1; i < argc; i++) vm.stack_manager.pop();

    int viewId = g_next_view_id++;
    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createScrollView", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, parentId);

    push_int_to_vm_stack(vm, viewId);
}

// Create CardView
void android_create_cardview(VM& vm, const uint8_t argc) {
    int parentId = -1;
    int elevation = 8; // default elevation in dp
    int cornerRadius = 8; // default corner radius in dp

    if (argc >= 3) {
        Value r = vm.stack_manager.pop();
        cornerRadius = (r.type == ValueType::INT) ? r.current_value.i : 8;
    }
    if (argc >= 2) {
        Value e = vm.stack_manager.pop();
        elevation = (e.type == ValueType::INT) ? e.current_value.i : 8;
    }
    if (argc >= 1) {
        Value p = vm.stack_manager.pop();
        parentId = (p.type == ValueType::INT) ? p.current_value.i : -1;
    }
    for (int i = 3; i < argc; i++) vm.stack_manager.pop();

    int viewId = g_next_view_id++;
    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createCardView", "(IIII)V");
    env->CallVoidMethod(droplet_activity, method, viewId, parentId, elevation, cornerRadius);

    push_int_to_vm_stack(vm, viewId);
}

// Create RecyclerView
void android_create_recyclerview(VM& vm, const uint8_t argc) {
    int parentId = -1;
    int layoutType = 0; // 0=vertical, 1=horizontal, 2=grid

    if (argc >= 2) {
        Value l = vm.stack_manager.pop();
        layoutType = (l.type == ValueType::INT) ? l.current_value.i : 0;
    }
    if (argc >= 1) {
        Value p = vm.stack_manager.pop();
        parentId = (p.type == ValueType::INT) ? p.current_value.i : -1;
    }
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = g_next_view_id++;
    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createRecyclerView", "(III)V");
    env->CallVoidMethod(droplet_activity, method, viewId, parentId, layoutType);

    push_int_to_vm_stack(vm, viewId);
}

// Add item to RecyclerView
void android_recyclerview_add_item(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value textVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    std::string text = textVal.toString();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "recyclerViewAddItem", "(ILjava/lang/String;)V");
    jstring jtext = env->NewStringUTF(text.c_str());
    env->CallVoidMethod(droplet_activity, method, viewId, jtext);
    env->DeleteLocalRef(jtext);

    vm.stack_manager.push(Value::createNIL());
}

// Clear RecyclerView items
void android_recyclerview_clear(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value idVal = vm.stack_manager.pop();
    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "recyclerViewClear", "(I)V");
    env->CallVoidMethod(droplet_activity, method, viewId);

    vm.stack_manager.push(Value::createNIL());
}

// Set view background color
void android_set_view_background_color(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value colorVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int color = (colorVal.type == ValueType::INT) ? colorVal.current_value.i : 0xFFFFFFFF;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewBackgroundColor", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, color);

    vm.stack_manager.push(Value::createNIL());
}

// Set view padding
void android_set_view_padding(VM& vm, const uint8_t argc) {
    if (argc < 5) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value bottom = vm.stack_manager.pop();
    Value right = vm.stack_manager.pop();
    Value top = vm.stack_manager.pop();
    Value left = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int l = (left.type == ValueType::INT) ? left.current_value.i : 0;
    int t = (top.type == ValueType::INT) ? top.current_value.i : 0;
    int r = (right.type == ValueType::INT) ? right.current_value.i : 0;
    int b = (bottom.type == ValueType::INT) ? bottom.current_value.i : 0;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewPadding", "(IIIII)V");
    env->CallVoidMethod(droplet_activity, method, viewId, l, t, r, b);

    vm.stack_manager.push(Value::createNIL());
}

// Set view size (width, height in dp)
void android_set_view_size(VM& vm, const uint8_t argc) {
    if (argc < 3) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value heightVal = vm.stack_manager.pop();
    Value widthVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int width = (widthVal.type == ValueType::INT) ? widthVal.current_value.i : -1;
    int height = (heightVal.type == ValueType::INT) ? heightVal.current_value.i : -1;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);
    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewSize", "(III)V");
    env->CallVoidMethod(droplet_activity, method, viewId, width, height);

    vm.stack_manager.push(Value::createNIL());
}

#endif