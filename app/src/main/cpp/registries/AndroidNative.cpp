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
    int userData; // ADD THIS - stores the index or any custom int
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

// Replace your existing android_create_button and onButtonClick functions with these:

void android_create_button(VM& vm, const uint8_t argc) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "create_button called with argc: %d", argc);

    if(argc < 2) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "create_button requires at least 2 args (got %d)", argc);
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    // Pop arguments in reverse order (last pushed = first popped)
    int userData = -1;
    if (argc >= 4) {
        Value userDataVal = vm.stack_manager.pop();
        userData = (userDataVal.type == ValueType::INT) ? userDataVal.current_value.i : -1;
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Button userData: %d", userData);
    }

    int parentId = -1;
    if (argc >= 3) {
        Value parent = vm.stack_manager.pop();
        parentId = (parent.type == ValueType::INT) ? parent.current_value.i : -1;
    }

    Value callback = vm.stack_manager.pop();
    Value title = vm.stack_manager.pop();

    // Pop any extra arguments
    for (int i = 4; i < argc; i++) {
        vm.stack_manager.pop();
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Button: %s, Callback type: %d, ParentId: %d, UserData: %d",
                        title.toString().c_str(), static_cast<int>(callback.type), parentId, userData);

    // Store callback info WITH userData
    int callbackId = g_next_callback_id++;
    CallbackInfo info;
    info.callback = callback;
    info.userData = userData;  // STORE THE USER DATA

    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        info.gcRoot = callback.current_value.object;
        g_callback_gc_roots.push_back(callback.current_value.object);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Stored GC root at %p", callback.current_value.object);
    } else {
        info.gcRoot = nullptr;
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Warning: callback is not an object!");
    }

    g_callbacks[callbackId] = info;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Created callback ID: %d with userData: %d", callbackId, userData);

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createButton", "(Ljava/lang/String;II)V");

    jstring jtitle = env->NewStringUTF(title.toString().c_str());
    env->CallVoidMethod(droplet_activity, method, jtitle, callbackId, parentId);
    env->DeleteLocalRef(jtitle);

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
    int userData = info.userData;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Found callback, type: %d, userData: %d",
                        static_cast<int>(callback.type), userData);

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

        // Create arguments vector with userData if it exists
        std::vector<Value> args;
        if (userData != -1) {
            args.push_back(Value::createINT(userData));
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Passing userData %d to callback", userData);
        }

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

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Adding item to RecyclerView %d: %s", viewId, text.c_str());

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

void android_set_toolbar_title(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value titleVal = vm.stack_manager.pop();
    for (int i = 1; i < argc; i++) vm.stack_manager.pop();

    std::string title = titleVal.toString();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setToolbarTitle", "(Ljava/lang/String;)V");
    jstring jtitle = env->NewStringUTF(title.c_str());
    env->CallVoidMethod(droplet_activity, method, jtitle);
    env->DeleteLocalRef(jtitle);

    vm.stack_manager.push(Value::createNIL());
}

// Create a new screen (returns screen ID)
void android_create_screen(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createINT(-1));
        return;
    }

    Value nameVal = vm.stack_manager.pop();
    for (int i = 1; i < argc; i++) vm.stack_manager.pop();

    std::string name = nameVal.toString();
    int screenId = g_next_view_id++;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createScreen", "(ILjava/lang/String;)V");
    jstring jname = env->NewStringUTF(name.c_str());
    env->CallVoidMethod(droplet_activity, method, screenId, jname);
    env->DeleteLocalRef(jname);

    push_int_to_vm_stack(vm, screenId);
}

// Navigate to a screen by ID
void android_navigate_to_screen(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value screenIdVal = vm.stack_manager.pop();
    for (int i = 1; i < argc; i++) vm.stack_manager.pop();

    int screenId = (screenIdVal.type == ValueType::INT) ? screenIdVal.current_value.i : -1;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "navigateToScreen", "(I)V");
    env->CallVoidMethod(droplet_activity, method, screenId);

    vm.stack_manager.push(Value::createNIL());
}

// Navigate back
void android_navigate_back(VM& vm, const uint8_t argc) {
    for (int i = 0; i < argc; i++) vm.stack_manager.pop();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "navigateBack", "()V");
    env->CallVoidMethod(droplet_activity, method);

    vm.stack_manager.push(Value::createNIL());
}

// Set back button visibility
void android_set_back_button_visible(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value visibleVal = vm.stack_manager.pop();
    for (int i = 1; i < argc; i++) vm.stack_manager.pop();

    int visible = (visibleVal.type == ValueType::INT) ? visibleVal.current_value.i : 0;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setBackButtonVisible", "(Z)V");
    env->CallVoidMethod(droplet_activity, method, visible != 0);

    vm.stack_manager.push(Value::createNIL());
}

void android_create_edittext(VM& vm, const uint8_t argc) {
    std::string hint = "";
    int parentId = -1;

    if (argc >= 2) {
        Value p = vm.stack_manager.pop();
        parentId = (p.type == ValueType::INT) ? p.current_value.i : -1;
    }
    if (argc >= 1) {
        Value h = vm.stack_manager.pop();
        hint = h.toString();
    }
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = g_next_view_id++;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createEditText", "(Ljava/lang/String;II)V");
    jstring jhint = env->NewStringUTF(hint.c_str());
    env->CallVoidMethod(droplet_activity, method, jhint, viewId, parentId);
    env->DeleteLocalRef(jhint);

    push_int_to_vm_stack(vm, viewId);
}

void android_get_edittext_value(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        ObjString* str = vm.allocator.allocate_string("");
        vm.stack_manager.push(Value::createOBJECT(str));
        return;
    }

    Value idVal = vm.stack_manager.pop();
    for (int i = 1; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "getEditTextValue", "(I)Ljava/lang/String;");
    jstring jresult = (jstring)env->CallObjectMethod(droplet_activity, method, viewId);

    std::string result = "";
    if (jresult != nullptr) {
        const char* str = env->GetStringUTFChars(jresult, nullptr);
        result = std::string(str);
        env->ReleaseStringUTFChars(jresult, str);
        env->DeleteLocalRef(jresult);
    }
    ObjString* str = vm.allocator.allocate_string(result);
    vm.stack_manager.push(Value::createOBJECT(str));
}

void android_set_edittext_hint(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value hintVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    std::string hint = hintVal.toString();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setEditTextHint", "(ILjava/lang/String;)V");
    jstring jhint = env->NewStringUTF(hint.c_str());
    env->CallVoidMethod(droplet_activity, method, viewId, jhint);
    env->DeleteLocalRef(jhint);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_edittext_input_type(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value typeVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int inputType = (typeVal.type == ValueType::INT) ? typeVal.current_value.i : 1;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setEditTextInputType", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, inputType);

    vm.stack_manager.push(Value::createNIL());
}

// ============================================
// STYLING FUNCTIONS
// ============================================

void android_set_text_size(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value sizeVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int size = (sizeVal.type == ValueType::INT) ? sizeVal.current_value.i : 16;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setTextSize", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, size);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_text_color(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value colorVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int color = (colorVal.type == ValueType::INT) ? colorVal.current_value.i : 0xFF000000;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setTextColor", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, color);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_text_style(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value styleVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int style = (styleVal.type == ValueType::INT) ? styleVal.current_value.i : 0;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setTextStyle", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, style);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_view_margin(VM& vm, const uint8_t argc) {
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
    jmethodID method = env->GetMethodID(cls, "setViewMargin", "(IIIII)V");
    env->CallVoidMethod(droplet_activity, method, viewId, l, t, r, b);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_view_gravity(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value gravityVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int gravity = (gravityVal.type == ValueType::INT) ? gravityVal.current_value.i : 0;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewGravity", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, gravity);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_view_elevation(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value elevVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int elevation = (elevVal.type == ValueType::INT) ? elevVal.current_value.i : 0;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewElevation", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, elevation);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_view_corner_radius(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value radiusVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 2; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int radius = (radiusVal.type == ValueType::INT) ? radiusVal.current_value.i : 0;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewCornerRadius", "(II)V");
    env->CallVoidMethod(droplet_activity, method, viewId, radius);

    vm.stack_manager.push(Value::createNIL());
}

void android_set_view_border(VM& vm, const uint8_t argc) {
    if (argc < 3) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value colorVal = vm.stack_manager.pop();
    Value widthVal = vm.stack_manager.pop();
    Value idVal = vm.stack_manager.pop();
    for (int i = 3; i < argc; i++) vm.stack_manager.pop();

    int viewId = (idVal.type == ValueType::INT) ? idVal.current_value.i : -1;
    int width = (widthVal.type == ValueType::INT) ? widthVal.current_value.i : 1;
    int color = (colorVal.type == ValueType::INT) ? colorVal.current_value.i : 0xFF000000;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "setViewBorder", "(III)V");
    env->CallVoidMethod(droplet_activity, method, viewId, width, color);

    vm.stack_manager.push(Value::createNIL());
}

// ============================================
// HTTP FUNCTIONS
// ============================================

// Replace your HTTP functions with these fixed versions that properly initialize userData

// HTTP GET: android_http_get(url, callback, headers_optional)
void android_http_get(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    // Pop headers if provided (optional JSON string)
    std::string headers = "";
    if (argc >= 3) {
        Value headersVal = vm.stack_manager.pop();
        headers = headersVal.toString();
    }

    Value callback = vm.stack_manager.pop();
    Value urlVal = vm.stack_manager.pop();

    for (int i = 3; i < argc; i++) vm.stack_manager.pop();

    std::string url = urlVal.toString();
    int callbackId = g_next_callback_id++;

    CallbackInfo info;
    info.callback = callback;
    info.userData = -1;  // INITIALIZE userData for HTTP callbacks
    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        info.gcRoot = callback.current_value.object;
        g_callback_gc_roots.push_back(callback.current_value.object);
    } else {
        info.gcRoot = nullptr;
    }
    g_callbacks[callbackId] = info;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "httpGet", "(Ljava/lang/String;ILjava/lang/String;)V");
    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jheaders = env->NewStringUTF(headers.c_str());
    env->CallVoidMethod(droplet_activity, method, jurl, callbackId, jheaders);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jheaders);

    vm.stack_manager.push(Value::createNIL());
}

// HTTP POST: android_http_post(url, body, callback, headers_optional)
void android_http_post(VM& vm, const uint8_t argc) {
    if (argc < 3) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    std::string headers = "";
    if (argc >= 4) {
        Value headersVal = vm.stack_manager.pop();
        headers = headersVal.toString();
    }

    Value callback = vm.stack_manager.pop();
    Value bodyVal = vm.stack_manager.pop();
    Value urlVal = vm.stack_manager.pop();

    for (int i = 4; i < argc; i++) vm.stack_manager.pop();

    std::string url = urlVal.toString();
    std::string body = bodyVal.toString();
    int callbackId = g_next_callback_id++;

    CallbackInfo info;
    info.callback = callback;
    info.userData = -1;  // INITIALIZE userData
    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        info.gcRoot = callback.current_value.object;
        g_callback_gc_roots.push_back(callback.current_value.object);
    } else {
        info.gcRoot = nullptr;
    }
    g_callbacks[callbackId] = info;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "httpPost", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V");
    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jbody = env->NewStringUTF(body.c_str());
    jstring jheaders = env->NewStringUTF(headers.c_str());
    env->CallVoidMethod(droplet_activity, method, jurl, jbody, callbackId, jheaders);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jbody);
    env->DeleteLocalRef(jheaders);

    vm.stack_manager.push(Value::createNIL());
}

// HTTP PUT: android_http_put(url, body, callback, headers_optional)
void android_http_put(VM& vm, const uint8_t argc) {
    if (argc < 3) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    std::string headers = "";
    if (argc >= 4) {
        Value headersVal = vm.stack_manager.pop();
        headers = headersVal.toString();
    }

    Value callback = vm.stack_manager.pop();
    Value bodyVal = vm.stack_manager.pop();
    Value urlVal = vm.stack_manager.pop();

    for (int i = 4; i < argc; i++) vm.stack_manager.pop();

    std::string url = urlVal.toString();
    std::string body = bodyVal.toString();
    int callbackId = g_next_callback_id++;

    CallbackInfo info;
    info.callback = callback;
    info.userData = -1;  // INITIALIZE userData
    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        info.gcRoot = callback.current_value.object;
        g_callback_gc_roots.push_back(callback.current_value.object);
    } else {
        info.gcRoot = nullptr;
    }
    g_callbacks[callbackId] = info;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "httpPut", "(Ljava/lang/String;Ljava/lang/String;ILjava/lang/String;)V");
    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jbody = env->NewStringUTF(body.c_str());
    jstring jheaders = env->NewStringUTF(headers.c_str());
    env->CallVoidMethod(droplet_activity, method, jurl, jbody, callbackId, jheaders);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jbody);
    env->DeleteLocalRef(jheaders);

    vm.stack_manager.push(Value::createNIL());
}

// HTTP DELETE: android_http_delete(url, callback, headers_optional)
void android_http_delete(VM& vm, const uint8_t argc) {
    if (argc < 2) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    std::string headers = "";
    if (argc >= 3) {
        Value headersVal = vm.stack_manager.pop();
        headers = headersVal.toString();
    }

    Value callback = vm.stack_manager.pop();
    Value urlVal = vm.stack_manager.pop();

    for (int i = 3; i < argc; i++) vm.stack_manager.pop();

    std::string url = urlVal.toString();
    int callbackId = g_next_callback_id++;

    CallbackInfo info;
    info.callback = callback;
    info.userData = -1;  // INITIALIZE userData
    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        info.gcRoot = callback.current_value.object;
        g_callback_gc_roots.push_back(callback.current_value.object);
    } else {
        info.gcRoot = nullptr;
    }
    g_callbacks[callbackId] = info;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "httpDelete", "(Ljava/lang/String;ILjava/lang/String;)V");
    jstring jurl = env->NewStringUTF(url.c_str());
    jstring jheaders = env->NewStringUTF(headers.c_str());
    env->CallVoidMethod(droplet_activity, method, jurl, callbackId, jheaders);
    env->DeleteLocalRef(jurl);
    env->DeleteLocalRef(jheaders);

    vm.stack_manager.push(Value::createNIL());
}

// HTTP Response callback (called from Java)
extern "C"
JNIEXPORT void JNICALL
Java_com_mist_example_MainActivity_onHttpResponse(JNIEnv* env, jobject thiz,
                                                  jint callbackId,
                                                  jboolean success,
                                                  jstring response,
                                                  jint statusCode) {
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "HTTP response received for callback %d", callbackId);

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

    const char* responseStr = env->GetStringUTFChars(response, nullptr);
    std::string responseData = std::string(responseStr);
    env->ReleaseStringUTFChars(response, responseStr);

    try {
        // Create arguments: success (bool), response (string), statusCode (int)
        std::vector<Value> args;
        args.push_back(Value::createINT(success ? 1 : 0));

        ObjString* responseObj = g_vm_instance->allocator.allocate_string(responseData);
        args.push_back(Value::createOBJECT(responseObj));

        args.push_back(Value::createINT(statusCode));

        bool execSuccess = g_vm_instance->execute_callback(callback, args);

        if (execSuccess) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "HTTP callback executed successfully");
        } else {
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "HTTP callback execution failed");
        }

    } catch (const std::exception& e) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "Exception in HTTP callback: %s", e.what());
    }
}

void android_clear_screen(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value screenIdVal = vm.stack_manager.pop();
    for (int i = 1; i < argc; i++) vm.stack_manager.pop();

    int screenId = (screenIdVal.type == ValueType::INT) ? screenIdVal.current_value.i : -1;

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "clearScreen", "(I)V");
    env->CallVoidMethod(droplet_activity, method, screenId);

    vm.stack_manager.push(Value::createNIL());
}
#endif