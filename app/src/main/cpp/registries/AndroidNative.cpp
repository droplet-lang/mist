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

JNIEXPORT void JNICALL
Java_com_mist_example_MainActivity_registerVM(JNIEnv* env, jobject thiz) {
    env->GetJavaVM(&droplet_java_vm);
    droplet_activity = env->NewGlobalRef(thiz);
}
}

// Global VM reference and callback storage
static VM* g_vm_instance = nullptr;

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
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "create_button requires 2 args (got %d)", argc);
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value callback = vm.stack_manager.pop();
    Value title = vm.stack_manager.pop();

    // Pop any extra arguments
    for (int i = 2; i < argc; i++) {
        vm.stack_manager.pop();
    }

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Button: %s, Callback type: %d",
                        title.toString().c_str(), static_cast<int>(callback.type));

    // Log detailed callback information AT STORAGE TIME
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "=== STORING CALLBACK ===");
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Callback type: %d", static_cast<int>(callback.type));
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Callback object ptr: %p",
                        (callback.type == ValueType::OBJECT) ? callback.current_value.object : nullptr);

    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        auto* boundMethod = dynamic_cast<ObjBoundMethod*>(callback.current_value.object);
        if (boundMethod) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Callback is BoundMethod, methodIndex: %d",
                                boundMethod->methodIndex);
        }

        auto* fnObj = dynamic_cast<ObjFunction*>(callback.current_value.object);
        if (fnObj) {
            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Callback is Function, functionIndex: %d",
                                fnObj->functionIndex);
        }
    } else {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Callback is not an object! Type: %d, storing as-is",
                            static_cast<int>(callback.type));
    }

    // Store callback info
    int callbackId = g_next_callback_id++;
    CallbackInfo info;
    info.callback = callback;  // Store the Value directly - it should be ObjBoundMethod or ObjFunction

    // CRITICAL: Keep the object alive by storing it as a GC root
    if (callback.type == ValueType::OBJECT && callback.current_value.object) {
        info.gcRoot = callback.current_value.object;
        g_callback_gc_roots.push_back(callback.current_value.object);
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Stored GC root at %p", callback.current_value.object);
    } else {
        info.gcRoot = nullptr;
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "Warning: callback is not an object!");
    }

    g_callbacks[callbackId] = info;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Registered callback ID: %d", callbackId);

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetMethodID(cls, "createButton", "(Ljava/lang/String;I)V");

    jstring jtitle = env->NewStringUTF(title.toString().c_str());
    env->CallVoidMethod(droplet_activity, method, jtitle, callbackId);
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

static int g_next_view_id = 1000; // start from 1000 to avoid collision

void push_int_to_vm_stack(VM& vm, int n) {
    // Replace with your actual Value creation for integers
    vm.stack_manager.push(Value::createINT(n));
}

// Create TextView: signature in Droplet: android_create_textview("Hello", parentId_opt?)
void android_create_textview(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        // pop whatever present
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value textVal = vm.stack_manager.pop();
    int parentId = -1;
    if (argc >= 2) {
        Value parentVal = vm.stack_manager.pop();
        parentId = (parentVal.type == ValueType::INT) ? parentVal.current_value.i : -1;
    }
    // pop extras
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

// Create ImageView: args: (pathOrUrl, parentId_opt, width, height)
void android_create_imageview(VM& vm, const uint8_t argc) {
    if (argc < 1) {
        for (int i = 0; i < argc; i++) vm.stack_manager.pop();
        vm.stack_manager.push(Value::createNIL());
        return;
    }

    Value pathVal = vm.stack_manager.pop();
    int parentId = -1, width = -1, height = -1;
    if (argc >= 2) {
        Value p = vm.stack_manager.pop(); parentId = (p.type == ValueType::INT) ? p.current_value.i : -1;
    }
    if (argc >= 3) {
        Value w = vm.stack_manager.pop(); width = (w.type == ValueType::INT) ? w.current_value.i : -1;
    }
    if (argc >= 4) {
        Value h = vm.stack_manager.pop(); height = (h.type == ValueType::INT) ? h.current_value.i : -1;
    }

    std::string path = pathVal.toString();
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


#endif