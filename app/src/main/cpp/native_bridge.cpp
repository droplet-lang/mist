#include <jni.h>
#include <string>
#include "droplet_vm_wrapper.cpp"

extern "C" {
    JNIEXPORT void JNICALL
    Java_com_mist_example_DropletVM_runBytecode(JNIEnv *env, jobject thiz, jstring path) {
        const char *bytecodePath = env->GetStringUTFChars(path, nullptr);

        DropletVMWrapper vm;
        vm.runBytecode(bytecodePath);

        env->ReleaseStringUTFChars(path, bytecodePath);
    }
}
