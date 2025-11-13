package com.mist.example

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import com.mist.example.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private val dropletVM = DropletVM()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Load bytecode from assets
        val assetFile = "hello.dbc"
        val file = filesDir.resolve(assetFile)
        assets.open(assetFile).use { input ->
            file.outputStream().use { output -> input.copyTo(output) }
        }

        dropletVM.runBytecode(file.absolutePath)
    }
}