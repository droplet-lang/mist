package com.mist.example

import android.os.Bundle
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.cardview.widget.CardView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.GridLayoutManager
import android.graphics.BitmapFactory
import android.graphics.Color
import android.util.TypedValue
import java.io.File
import java.io.InputStream
import java.net.URL
import java.net.HttpURLConnection
import java.util.concurrent.Executors

class MainActivity : AppCompatActivity() {
    private lateinit var container: LinearLayout
    private val viewMap = HashMap<Int, View>()
    private val recyclerAdapters = HashMap<Int, SimpleRecyclerAdapter>()

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

        registerVM()

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


    fun createButton(title: String, callbackId: Int, parentId: Int) {
        runOnUiThread {
            val button = Button(this).apply {
                text = title
                setOnClickListener { onButtonClick(callbackId) }
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                ).apply {
                    setMargins(8, 8, 8, 8)
                }
            }

            // Add to specified parent or default container
            if (parentId != -1 && viewMap.containsKey(parentId)) {
                val parent = viewMap[parentId] as? ViewGroup
                parent?.addView(button)
            } else {
                container.addView(button)
            }
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

    fun createImageView(pathOrUrl: String?, viewId: Int, parentId: Int, width: Int, height: Int) {
        runOnUiThread {
            val iv = ImageView(this)
            val w = if (width > 0) dpToPx(width) else ViewGroup.LayoutParams.WRAP_CONTENT
            val h = if (height > 0) dpToPx(height) else ViewGroup.LayoutParams.WRAP_CONTENT
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

    fun createScrollView(viewId: Int, parentId: Int) {
        runOnUiThread {
            val scrollView = ScrollView(this)
            val params = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            scrollView.layoutParams = params

            // Create a child container for content
            val innerLayout = LinearLayout(this)
            innerLayout.orientation = LinearLayout.VERTICAL
            scrollView.addView(innerLayout)

            viewMap[viewId] = innerLayout // Store the inner layout for adding children
            viewMap[viewId + 1000000] = scrollView // Store scrollview with offset key

            if (parentId != -1 && viewMap.containsKey(parentId)) {
                val parent = viewMap[parentId] as? ViewGroup
                parent?.addView(scrollView)
            } else {
                container.addView(scrollView)
            }
        }
    }

    fun createCardView(viewId: Int, parentId: Int, elevation: Int, cornerRadius: Int) {
        runOnUiThread {
            val cardView = CardView(this)
            val params = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            params.setMargins(16, 16, 16, 16)
            cardView.layoutParams = params
            cardView.radius = dpToPx(cornerRadius).toFloat()
            cardView.cardElevation = dpToPx(elevation).toFloat()
            cardView.setContentPadding(16, 16, 16, 16)

            // Create inner layout for content
            val innerLayout = LinearLayout(this)
            innerLayout.orientation = LinearLayout.VERTICAL
            cardView.addView(innerLayout)

            viewMap[viewId] = innerLayout // Store inner layout for adding children
            viewMap[viewId + 2000000] = cardView // Store cardview with offset

            if (parentId != -1 && viewMap.containsKey(parentId)) {
                val parent = viewMap[parentId] as? ViewGroup
                parent?.addView(cardView)
            } else {
                container.addView(cardView)
            }
        }
    }

    fun createRecyclerView(viewId: Int, parentId: Int, layoutType: Int) {
        runOnUiThread {
            val recyclerView = RecyclerView(this)
            val params = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            recyclerView.layoutParams = params

            // Set layout manager based on type
            recyclerView.layoutManager = when(layoutType) {
                1 -> LinearLayoutManager(this, LinearLayoutManager.HORIZONTAL, false)
                2 -> GridLayoutManager(this, 2) // 2 columns for grid
                else -> LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false)
            }

            // Create and set adapter
            val adapter = SimpleRecyclerAdapter()
            recyclerView.adapter = adapter
            recyclerAdapters[viewId] = adapter

            viewMap[viewId] = recyclerView

            if (parentId != -1 && viewMap.containsKey(parentId)) {
                val parent = viewMap[parentId] as? ViewGroup
                parent?.addView(recyclerView)
            } else {
                container.addView(recyclerView)
            }
        }
    }

    fun recyclerViewAddItem(viewId: Int, text: String) {
        runOnUiThread {
            recyclerAdapters[viewId]?.addItem(text)
        }
    }

    fun recyclerViewClear(viewId: Int) {
        runOnUiThread {
            recyclerAdapters[viewId]?.clear()
        }
    }

    fun addViewToParent(parentId: Int, childId: Int) {
        runOnUiThread {
            val parent = viewMap[parentId] as? ViewGroup
            val child = viewMap[childId]
            if (parent != null && child != null) {
                parent.addView(child)
            }
        }
    }

    fun setViewText(viewId: Int, text: String) {
        runOnUiThread {
            val v = viewMap[viewId]
            if (v is TextView) {
                v.text = text
            }
        }
    }

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

    fun setViewImage(viewId: Int, pathOrUrl: String) {
        runOnUiThread {
            val v = viewMap[viewId]
            if (v is ImageView) {
                loadImageIntoView(v, pathOrUrl)
            }
        }
    }

    fun setViewBackgroundColor(viewId: Int, color: Int) {
        runOnUiThread {
            viewMap[viewId]?.setBackgroundColor(color)
            viewMap[viewId + 1000000]?.setBackgroundColor(color) // For ScrollView
//            viewMap[viewId + 2000000]?.seCard(color) // For CardView
        }
    }

    fun setViewPadding(viewId: Int, left: Int, top: Int, right: Int, bottom: Int) {
        runOnUiThread {
            val l = dpToPx(left)
            val t = dpToPx(top)
            val r = dpToPx(right)
            val b = dpToPx(bottom)
            viewMap[viewId]?.setPadding(l, t, r, b)
        }
    }

    fun setViewSize(viewId: Int, width: Int, height: Int) {
        runOnUiThread {
            val view = viewMap[viewId] ?: return@runOnUiThread
            val w = if (width == -1) ViewGroup.LayoutParams.WRAP_CONTENT
            else if (width == -2) ViewGroup.LayoutParams.MATCH_PARENT
            else dpToPx(width)
            val h = if (height == -1) ViewGroup.LayoutParams.WRAP_CONTENT
            else if (height == -2) ViewGroup.LayoutParams.MATCH_PARENT
            else dpToPx(height)

            view.layoutParams = LinearLayout.LayoutParams(w, h)
        }
    }

    private fun dpToPx(dp: Int): Int {
        return TypedValue.applyDimension(
            TypedValue.COMPLEX_UNIT_DIP,
            dp.toFloat(),
            resources.displayMetrics
        ).toInt()
    }

    private fun loadImageIntoView(iv: ImageView, pathOrUrl: String) {
        if (pathOrUrl.startsWith("http://") || pathOrUrl.startsWith("https://")) {
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
            try {
                val bmp = BitmapFactory.decodeFile(pathOrUrl)
                if (bmp != null) {
                    iv.setImageBitmap(bmp)
                    return
                }
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

// Simple RecyclerView Adapter
class SimpleRecyclerAdapter : RecyclerView.Adapter<SimpleRecyclerAdapter.ViewHolder>() {
    private val items = mutableListOf<String>()

    class ViewHolder(val textView: TextView) : RecyclerView.ViewHolder(textView)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val textView = TextView(parent.context).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            setPadding(32, 32, 32, 32)
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
        }
        return ViewHolder(textView)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.textView.text = items[position]
    }

    override fun getItemCount() = items.size

    fun addItem(text: String) {
        items.add(text)
        notifyItemInserted(items.size - 1)
    }

    fun clear() {
        val size = items.size
        items.clear()
        notifyItemRangeRemoved(0, size)
    }
}
