#include "droplet_vm_wrapper.h"
#include "droplet/src/vm/Loader.h"
#include "droplet/src/native/Native.h"
#include "droplet/src/native/NativeRegisteries.h"
#include "registries/AndroidNative.h"
#include "registries/AndroidRegistries.h"
#include <android/log.h>
#include <memory>
#include <mutex>

class DropletVMWrapperImpl {
public:
    std::unique_ptr<VM> vm;

    DropletVMWrapperImpl() {
        vm = std::make_unique<VM>();
        android_set_vm_instance(vm.get());
        initCoreBuiltins();
        initAndroidBuiltins();
        register_native_functions(*vm);
        register_android_native_functions(*vm);
        __android_log_print(ANDROID_LOG_INFO, "Droplet", "VM created (singleton)");
    }

    ~DropletVMWrapperImpl() {
        android_set_vm_instance(nullptr);
        __android_log_print(ANDROID_LOG_INFO, "Droplet", "VM destroyed");
    }
};

// ---- Singleton holder ----
static std::unique_ptr<DropletVMWrapperImpl> s_impl;
static std::mutex s_mutex;

// ---- Wrapper Implementation ----
DropletVMWrapper* DropletVMWrapper::getInstance() {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_impl) {
        s_impl = std::make_unique<DropletVMWrapperImpl>();
    }
    static DropletVMWrapper instance;
    return &instance;
}

void DropletVMWrapper::destroyInstance() {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_impl.reset();
}

void DropletVMWrapper::runBytecode(const std::string &path) {
    if (!s_impl) return;

    Loader loader;
    auto& vm = *s_impl->vm;

    if (!loader.load_dbc_file(path, vm)) {
        __android_log_print(ANDROID_LOG_ERROR, "Droplet", "Failed to load %s", path.c_str());
        return;
    }

    uint32_t mainIdx = vm.get_function_index("main");
    if (mainIdx == UINT32_MAX) {
        __android_log_print(ANDROID_LOG_ERROR, "Droplet", "No main() found");
        return;
    }

    vm.call_function_by_index(mainIdx, 0);
    vm.run();
}

VM* DropletVMWrapper::getVM() {
    return s_impl ? s_impl->vm.get() : nullptr;
}
