#include "droplet/src/vm/VM.h"
#include "droplet/src/vm/Loader.h"
#include "droplet/src/native/Native.h"
#include "droplet/src/native/NativeRegisteries.h"
#include "registries/AndroidNative.h"
#include "registries/AndroidRegistries.h"
#include <string>
#include <android/log.h>

class DropletVMWrapper {
public:
    void runBytecode(const std::string &bytecodePath) {
        VM vm;
        Loader loader;

        initCoreBuiltins();
        initAndroidBuiltins();

        register_native_functions(vm);
        register_android_native_functions(vm);

        if (!loader.load_dbc_file(bytecodePath, vm)) {
            __android_log_print(ANDROID_LOG_ERROR, "Droplet", "Failed to load %s", bytecodePath.c_str());
            return;
        }

        uint32_t mainIdx = vm.get_function_index("main");
        if (mainIdx == UINT32_MAX) {
            __android_log_print(ANDROID_LOG_ERROR, "Droplet", "No main function found");
            return;
        }

        vm.call_function_by_index(mainIdx, 0);
        vm.run();
    }
};
