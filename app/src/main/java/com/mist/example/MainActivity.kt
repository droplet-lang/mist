package com.mist.example

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.TextView
import android.widget.Toast
import com.mist.example.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private val dropletVM = DropletVM()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        instance = this
        registerVM()

        // Load bytecode from assets
        val assetFile = "hello.dbc"
        val file = filesDir.resolve(assetFile)
        assets.open(assetFile).use { input ->
            file.outputStream().use { output -> input.copyTo(output) }
        }

        dropletVM.runBytecode(file.absolutePath)
    }

    external fun registerVM()

    companion object {
        @JvmStatic
        fun showToast(msg: String) {
            instance?.runOnUiThread {
                Toast.makeText(instance, msg, Toast.LENGTH_SHORT).show()
            }
        }

        var instance: MainActivity? = null
    }
}