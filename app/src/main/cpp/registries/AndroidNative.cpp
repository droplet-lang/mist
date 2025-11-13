//
// Created by NITRO on 11/13/2025.
//

#include "AndroidNative.h"


#if defined(__ANDROID__)

#include <android/log.h>
#include <jni.h>
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

void android_native_toast(VM& vm, const uint8_t argc) {
    if (argc == 0) return;
    Value msg = vm.stack_manager.peek(0);
    std::string str = msg.toString();

    JNIEnv* env;
    droplet_java_vm->AttachCurrentThread(&env, nullptr);

    jclass cls = env->GetObjectClass(droplet_activity);
    jmethodID method = env->GetStaticMethodID(cls, "showToast", "(Ljava/lang/String;)V");

    jstring jmsg = env->NewStringUTF(str.c_str());
    env->CallStaticVoidMethod(cls, method, jmsg);
    env->DeleteLocalRef(jmsg);

    vm.stack_manager.push(Value::createNIL());
}

#endif