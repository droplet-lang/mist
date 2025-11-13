#ifndef DROPLET_VM_WRAPPER_H
#define DROPLET_VM_WRAPPER_H

#include "droplet/src/vm/VM.h"
#include <string>

class DropletVMWrapper {
public:
    static DropletVMWrapper* getInstance();
    static void destroyInstance();

    void runBytecode(const std::string &bytecodePath);
    VM* getVM();
};

#endif // DROPLET_VM_WRAPPER_H
