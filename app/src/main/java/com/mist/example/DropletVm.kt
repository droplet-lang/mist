package com.mist.example

class DropletVM {
    companion object {
        init {
            System.loadLibrary("droplet_native")
        }
    }

    external fun runBytecode(path: String)
}
