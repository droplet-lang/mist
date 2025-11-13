package com.mist.example

import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.LinearLayout
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import android.graphics.BitmapFactory
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import java.net.URL
import java.util.concurrent.Executors
import android.util.TypedValue
import java.io.InputStream
import java.net.HttpURLConnection

class MainActivity : AppCompatActivity() {
    private lateinit var container: LinearLayout
    private val viewMap = HashMap<Int, View>()

    companion object {
        init {
            System.loadLibrary("droplet_native")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        container = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(16, 16, 16, 16)
        }
        setContentView(container)

        // Initialize VM singleton immediately
        registerVM()

        // Run Droplet bytecode
        val vm = DropletVM()
        val assetName = "bundle.dbc"
        val outFile = File(filesDir, assetName)
        assets.open(assetName).use { input ->
            outFile.outputStream().use { output -> input.copyTo(output) }
        }
        vm.runBytecode(outFile.absolutePath)
    }

    fun showToast(message: String) {
        runOnUiThread { Toast.makeText(this, message, Toast.LENGTH_SHORT).show() }
    }

    fun createButton(title: String, callbackId: Int) {
        runOnUiThread {
            val button = Button(this).apply {
                text = title
                setOnClickListener { onButtonClick(callbackId) }
            }
            container.addView(button)
        }
    }

    fun createTextView(text: String?, viewId: Int, parentId: Int) {
        runOnUiThread {
            val tv = TextView(this)
            tv.text = text ?: ""
            tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            val params = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            params.setMargins(8, 8, 8, 8)
            tv.layoutParams = params
            viewMap[viewId] = tv

            if (parentId != -1 && viewMap.containsKey(parentId)) {
                val parent = viewMap[parentId] as? ViewGroup
                parent?.addView(tv)
            } else {
                container.addView(tv)
            }
        }
    }

    // Create ImageView (pathOrUrl): width/height optional (pass -1 for wrap_content)
    fun createImageView(pathOrUrl: String?, viewId: Int, parentId: Int, width: Int, height: Int) {
        runOnUiThread {
            val iv = ImageView(this)
            val w = if (width > 0) width else ViewGroup.LayoutParams.WRAP_CONTENT
            val h = if (height > 0) height else ViewGroup.LayoutParams.WRAP_CONTENT
            val params = LinearLayout.LayoutParams(w, h)
            params.setMargins(8, 8, 8, 8)
            iv.layoutParams = params
            iv.adjustViewBounds = true
            iv.scaleType = ImageView.ScaleType.CENTER_CROP
            viewMap[viewId] = iv

            if (!pathOrUrl.isNullOrEmpty()) {
                loadImageIntoView(iv, pathOrUrl)
            }

            if (parentId != -1 && viewMap.containsKey(parentId)) {
                val parent = viewMap[parentId] as? ViewGroup
                parent?.addView(iv)
            } else {
                container.addView(iv)
            }
        }
    }

    // Create LinearLayout (orientation: 0 vertical, 1 horizontal)
    fun createLinearLayout(orientation: Int, viewId: Int, parentId: Int) {
        runOnUiThread {
            val layout = LinearLayout(this)
            layout.orientation = if (orientation == 1) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL
            val params = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            layout.layoutParams = params
            viewMap[viewId] = layout

            if (parentId != -1 && viewMap.containsKey(parentId)) {
                val parent = viewMap[parentId] as? ViewGroup
                parent?.addView(layout)
            } else {
                container.addView(layout)
            }
        }
    }

    // Add child to parent (both by id)
    fun addViewToParent(parentId: Int, childId: Int) {
        runOnUiThread {
            val parent = viewMap[parentId] as? ViewGroup
            val child = viewMap[childId]
            if (parent != null && child != null) {
                parent.addView(child)
            }
        }
    }

    // Set view text
    fun setViewText(viewId: Int, text: String) {
        runOnUiThread {
            val v = viewMap[viewId]
            if (v is TextView) {
                v.text = text
            }
        }
    }

    // Set view visibility: 0=VISIBLE,1=INVISIBLE,2=GONE
    fun setViewVisibility(viewId: Int, visibility: Int) {
        runOnUiThread {
            val v = viewMap[viewId]
            v?.visibility = when (visibility) {
                1 -> View.INVISIBLE
                2 -> View.GONE
                else -> View.VISIBLE
            }
        }
    }

    // Set view image (pathOrUrl). If http(s) fetch network, else treat as file path.
    fun setViewImage(viewId: Int, pathOrUrl: String) {
        runOnUiThread {
            val v = viewMap[viewId]
            if (v is ImageView) {
                loadImageIntoView(v, pathOrUrl)
            }
        }
    }

    private fun loadImageIntoView(iv: ImageView, pathOrUrl: String) {
        if (pathOrUrl.startsWith("http://") || pathOrUrl.startsWith("https://")) {
            // load from network in background thread
            val executor = Executors.newSingleThreadExecutor()
            executor.execute {
                try {
                    val url = URL(pathOrUrl)
                    val conn = url.openConnection() as HttpURLConnection
                    conn.doInput = true
                    conn.connect()
                    val input: InputStream = conn.inputStream
                    val bmp = BitmapFactory.decodeStream(input)
                    runOnUiThread { iv.setImageBitmap(bmp) }
                } catch (e: Exception) {
                    e.printStackTrace()
                }
            }
        } else {
            // local file path or asset path: try file
            try {
                val bmp = BitmapFactory.decodeFile(pathOrUrl)
                if (bmp != null) {
                    iv.setImageBitmap(bmp)
                    return
                }
                // fallback: assets - try opening from assets
                val stream = assets.open(pathOrUrl)
                val bmp2 = BitmapFactory.decodeStream(stream)
                iv.setImageBitmap(bmp2)
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        DropletVM().cleanup()
    }

    private external fun registerVM()
    private external fun onButtonClick(callbackId: Int)
}
