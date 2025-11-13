package com.mist.example

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.widget.Button
import android.widget.LinearLayout
import android.widget.Toast

class MainActivity : AppCompatActivity() {

    private val dropletVM = DropletVM()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Set content view to XML layout with root LinearLayout
        setContentView(R.layout.activity_main)

        // Set instance and VM references
        instance = this
        dropletVMInstance = dropletVM

        // Register Droplet VM
        registerVM()

        // Load bytecode from assets to internal storage
        val assetFile = "hello.dbc"
        val file = filesDir.resolve(assetFile)
        assets.open(assetFile).use { input ->
            file.outputStream().use { output ->
                input.copyTo(output)
            }
        }

        // Run bytecode
        dropletVM.runBytecode(file.absolutePath)
    }

    external fun registerVM()

    companion object {
        @JvmStatic
        var instance: MainActivity? = null

        @JvmStatic
        var dropletVMInstance: DropletVM? = null

        // Show a toast message
        @JvmStatic
        fun showToast(msg: String) {
            instance?.runOnUiThread {
                Toast.makeText(instance, msg, Toast.LENGTH_SHORT).show()
            }
        }

        // Create a button dynamically
        @JvmStatic
        fun createButton(title: String, callbackId: Int) {
            instance?.runOnUiThread {
                val button = Button(instance)
                button.text = title

                button.setOnClickListener {
                    // Call the Droplet VM callback by index
//                    dropletVMInstance?.callFunctionByIndex(callbackId, 0)
                }

                // Add the button to root layout
                instance?.findViewById<LinearLayout>(R.id.rootLayout)?.addView(button)
            }
        }
    }
}
