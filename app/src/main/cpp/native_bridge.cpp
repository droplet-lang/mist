#include <jni.h>
#include "droplet_vm_wrapper.h"

extern "C" {

JNIEXPORT void JNICALL
Java_com_mist_example_DropletVM_runBytecode(JNIEnv *env, jobject thiz, jstring path) {
    const char *bytecodePath = env->GetStringUTFChars(path, nullptr);
    DropletVMWrapper::getInstance()->runBytecode(bytecodePath);
    env->ReleaseStringUTFChars(path, bytecodePath);
}

JNIEXPORT void JNICALL
Java_com_mist_example_DropletVM_cleanup(JNIEnv *env, jobject thiz) {
    DropletVMWrapper::destroyInstance();
}
}

