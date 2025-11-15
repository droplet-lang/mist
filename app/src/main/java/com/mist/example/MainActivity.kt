package com.mist.example

import android.os.Bundle
import android.view.MenuItem
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.cardview.widget.CardView
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import androidx.recyclerview.widget.GridLayoutManager
import android.graphics.BitmapFactory
import android.util.TypedValue
import android.util.Log
import java.io.File
import java.io.InputStream
import java.net.URL
import java.net.HttpURLConnection
import java.util.concurrent.Executors
import java.util.Stack
 import android.text.InputType
 import android.graphics.Typeface
 import android.view.Gravity
 import android.graphics.drawable.GradientDrawable
 import android.graphics.drawable.ColorDrawable
 import android.graphics.Color
 import android.widget.EditText

class MainActivity : AppCompatActivity() {
    private lateinit var toolbar: Toolbar
    private lateinit var contentFrame: FrameLayout
    private val viewMap = HashMap<Int, View>()
    private val screenMap = HashMap<Int, ScreenInfo>()
    private val recyclerAdapters = HashMap<Int, SimpleRecyclerAdapter>()
    private val navigationStack = Stack<Int>()
    private var currentScreenId: Int = -1

    data class ScreenInfo(
        val id: Int,
        val name: String,
        val container: LinearLayout
    )

    companion object {
        private const val TAG = "MainActivity"
        init {
            System.loadLibrary("droplet_native")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        // Setup toolbar
        toolbar = findViewById(R.id.toolbar)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(false)
        supportActionBar?.title = "Droplet App"

        // Setup content frame
        contentFrame = findViewById(R.id.content_frame)

        // Create default screen
        val defaultContainer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
            setPadding(16, 16, 16, 16)
        }
        contentFrame.addView(defaultContainer)

        val defaultScreen = ScreenInfo(-1, "Main", defaultContainer)
        screenMap[-1] = defaultScreen
        viewMap[-1] = defaultContainer
        currentScreenId = -1

        registerVM()

        val vm = DropletVM()
        val assetName = "bundle.dbc"
        val outFile = File(filesDir, assetName)
        assets.open(assetName).use { input ->
            outFile.outputStream().use { output -> input.copyTo(output) }
        }
        vm.runBytecode(outFile.absolutePath)
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            android.R.id.home -> {
                navigateBack()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        if (navigationStack.isEmpty()) {
            // On home screen - show toast instead of closing
            Toast.makeText(this, "Already on home screen", Toast.LENGTH_SHORT).show()
        } else {
            navigateBack()
        }
    }

    fun setToolbarTitle(title: String) {
        runOnUiThread {
            supportActionBar?.title = title
        }
    }

    fun setBackButtonVisible(visible: Boolean) {
        runOnUiThread {
            supportActionBar?.setDisplayHomeAsUpEnabled(visible)
        }
    }

    fun createScreen(screenId: Int, name: String) {
        runOnUiThread {
            Log.d(TAG, "Creating screen: id=$screenId, name=$name")
            val container = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams = FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT
                )
                setPadding(16, 16, 16, 16)
                visibility = View.GONE
            }
            contentFrame.addView(container)

            val screen = ScreenInfo(screenId, name, container)
            screenMap[screenId] = screen
            viewMap[screenId] = container
            Log.d(TAG, "Screen created successfully: $screenId")
        }
    }

    fun navigateToScreen(screenId: Int) {
        runOnUiThread {
            Log.d(TAG, "Navigating to screen: $screenId from $currentScreenId")
            val screen = screenMap[screenId]
            if (screen == null) {
                Log.e(TAG, "Screen not found: $screenId")
                return@runOnUiThread
            }

            // FIX: Always hide current screen, including the default screen
            screenMap[currentScreenId]?.container?.visibility = View.GONE

            // Push current screen to stack
            navigationStack.push(currentScreenId)
            Log.d(TAG, "Pushed $currentScreenId to stack, stack size: ${navigationStack.size}")

            // Show new screen
            screen.container.visibility = View.VISIBLE
            currentScreenId = screenId

            // Update toolbar with back button
            supportActionBar?.title = screen.name
            supportActionBar?.setDisplayHomeAsUpEnabled(true)
            Log.d(TAG, "Navigation complete. Current screen: $currentScreenId, Stack: $navigationStack")
        }
    }

    fun navigateBack() {
        runOnUiThread {
            Log.d(TAG, "Navigate back called. Stack size: ${navigationStack.size}")
            if (navigationStack.isEmpty()) {
                Log.d(TAG, "Stack empty, ignoring")
                return@runOnUiThread
            }

            // Hide current screen
            screenMap[currentScreenId]?.container?.visibility = View.GONE

            // Show previous screen
            val previousScreenId = navigationStack.pop()
            currentScreenId = previousScreenId
            val screen = screenMap[previousScreenId]
            screen?.container?.visibility = View.VISIBLE

            // Update toolbar
            supportActionBar?.title = screen?.name ?: "Main"
            supportActionBar?.setDisplayHomeAsUpEnabled(navigationStack.isNotEmpty())
            Log.d(TAG, "Navigated back to: $currentScreenId, Stack size: ${navigationStack.size}")
        }
    }

    private fun getTargetContainer(parentId: Int): ViewGroup {
        if (parentId != -1) {
            val parent = viewMap[parentId]
            if (parent is ViewGroup) {
                Log.d(TAG, "Using parent container: $parentId")
                return parent
            }
            Log.w(TAG, "Parent $parentId not found or not ViewGroup, using current screen")
        }

        val container = screenMap[currentScreenId]?.container ?: screenMap[-1]!!.container
        Log.d(TAG, "Using current screen container: $currentScreenId")
        return container
    }

    fun showToast(message: String) {
        runOnUiThread { Toast.makeText(this, message, Toast.LENGTH_SHORT).show() }
    }

    fun createButton(title: String, callbackId: Int, parentId: Int) {
        runOnUiThread {
            Log.d(TAG, "Creating button: '$title', callback=$callbackId, parent=$parentId")
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

            // Use the same pattern as other view creation methods
            val parent = if (parentId != -1 && viewMap.containsKey(parentId)) {
                val p = viewMap[parentId]
                if (p is ViewGroup) {
                    Log.d(TAG, "Adding button to parent: $parentId")
                    p
                } else {
                    Log.w(TAG, "Parent $parentId is not a ViewGroup, using target container")
                    getTargetContainer(parentId)
                }
            } else {
                Log.d(TAG, "No valid parent, using target container")
                getTargetContainer(parentId)
            }

            parent.addView(button)
        }
    }


    // ... rest of the methods remain the same ...
    fun createTextView(text: String?, viewId: Int, parentId: Int) {
        runOnUiThread {
            Log.d(TAG, "Creating TextView: id=$viewId, parent=$parentId, text='$text'")
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

            getTargetContainer(parentId).addView(tv)
        }
    }

    fun createImageView(pathOrUrl: String?, viewId: Int, parentId: Int, width: Int, height: Int) {
        runOnUiThread {
            Log.d(TAG, "Creating ImageView: id=$viewId, parent=$parentId")
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

            getTargetContainer(parentId).addView(iv)
        }
    }

    fun createLinearLayout(orientation: Int, viewId: Int, parentId: Int) {
        runOnUiThread {
            Log.d(TAG, "Creating LinearLayout: id=$viewId, parent=$parentId, orientation=$orientation")
            val layout = LinearLayout(this)
            layout.orientation = if (orientation == 1) LinearLayout.HORIZONTAL else LinearLayout.VERTICAL
            val params = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
            layout.layoutParams = params
            viewMap[viewId] = layout

            getTargetContainer(parentId).addView(layout)
        }
    }

    fun createScrollView(viewId: Int, parentId: Int) {
        runOnUiThread {
            Log.d(TAG, "Creating ScrollView: id=$viewId, parent=$parentId")
            val scrollView = ScrollView(this)
            val params = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            scrollView.layoutParams = params

            val innerLayout = LinearLayout(this)
            innerLayout.orientation = LinearLayout.VERTICAL
            scrollView.addView(innerLayout)

            viewMap[viewId] = innerLayout
            viewMap[viewId + 1000000] = scrollView

            getTargetContainer(parentId).addView(scrollView)
        }
    }

    fun createCardView(viewId: Int, parentId: Int, elevation: Int, cornerRadius: Int) {
        runOnUiThread {
            Log.d(TAG, "Creating CardView: id=$viewId, parent=$parentId")
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

            val innerLayout = LinearLayout(this)
            innerLayout.orientation = LinearLayout.VERTICAL
            cardView.addView(innerLayout)

            viewMap[viewId] = innerLayout
            viewMap[viewId + 2000000] = cardView

            getTargetContainer(parentId).addView(cardView)
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

            recyclerView.layoutManager = when(layoutType) {
                1 -> LinearLayoutManager(this, LinearLayoutManager.HORIZONTAL, false)
                2 -> GridLayoutManager(this, 2)
                else -> LinearLayoutManager(this, LinearLayoutManager.VERTICAL, false)
            }

            val adapter = SimpleRecyclerAdapter()
            recyclerView.adapter = adapter
            recyclerAdapters[viewId] = adapter

            viewMap[viewId] = recyclerView

            getTargetContainer(parentId).addView(recyclerView)
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
            viewMap[viewId + 1000000]?.setBackgroundColor(color)
            viewMap[viewId + 2000000]?.setBackgroundColor(color)
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

    fun createEditText(hint: String, viewId: Int, parentId: Int) {
        runOnUiThread {
            Log.d(TAG, "Creating EditText: id=$viewId, parent=$parentId, hint='$hint'")
            val editText = EditText(this).apply {
                this.hint = hint
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
                setPadding(dpToPx(12), dpToPx(12), dpToPx(12), dpToPx(12))
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                ).apply {
                    setMargins(8, 8, 8, 8)
                }
            }
            viewMap[viewId] = editText
            getTargetContainer(parentId).addView(editText)
        }
    }

    fun getEditTextValue(viewId: Int): String {
        val view = viewMap[viewId]
        return if (view is EditText) {
            view.text.toString()
        } else {
            ""
        }
    }

    fun setEditTextHint(viewId: Int, hint: String) {
        runOnUiThread {
            val view = viewMap[viewId]
            if (view is EditText) {
                view.hint = hint
            }
        }
    }

    fun setEditTextInputType(viewId: Int, inputType: Int) {
        runOnUiThread {
            val view = viewMap[viewId]
            if (view is EditText) {
                view.inputType = when(inputType) {
                    1 -> InputType.TYPE_CLASS_TEXT
                    2 -> InputType.TYPE_CLASS_NUMBER
                    3 -> InputType.TYPE_TEXT_VARIATION_PASSWORD
                    4 -> InputType.TYPE_TEXT_VARIATION_EMAIL_ADDRESS
                    5 -> InputType.TYPE_CLASS_PHONE
                    6 -> InputType.TYPE_TEXT_FLAG_MULTI_LINE
                    else -> InputType.TYPE_CLASS_TEXT
                }
            }
        }
    }

// ============================================
// STYLING METHODS
// ============================================

    fun setTextSize(viewId: Int, size: Int) {
        runOnUiThread {
            val view = viewMap[viewId]
            when (view) {
                is TextView -> view.setTextSize(TypedValue.COMPLEX_UNIT_SP, size.toFloat())
                is Button -> view.setTextSize(TypedValue.COMPLEX_UNIT_SP, size.toFloat())
                is EditText -> view.setTextSize(TypedValue.COMPLEX_UNIT_SP, size.toFloat())
            }
        }
    }

    fun setTextColor(viewId: Int, color: Int) {
        runOnUiThread {
            val view = viewMap[viewId]
            when (view) {
                is TextView -> view.setTextColor(color)
                is Button -> view.setTextColor(color)
                is EditText -> view.setTextColor(color)
            }
        }
    }

    fun setTextStyle(viewId: Int, style: Int) {
        runOnUiThread {
            val view = viewMap[viewId]
            val typeface = when(style) {
                1 -> Typeface.BOLD
                2 -> Typeface.ITALIC
                3 -> Typeface.BOLD_ITALIC
                else -> Typeface.NORMAL
            }

            when (view) {
                is TextView -> view.setTypeface(null, typeface)
                is Button -> view.setTypeface(null, typeface)
                is EditText -> view.setTypeface(null, typeface)
            }
        }
    }

    fun setViewMargin(viewId: Int, left: Int, top: Int, right: Int, bottom: Int) {
        runOnUiThread {
            val view = viewMap[viewId] ?: return@runOnUiThread
            val l = dpToPx(left)
            val t = dpToPx(top)
            val r = dpToPx(right)
            val b = dpToPx(bottom)

            val params = view.layoutParams
            if (params is ViewGroup.MarginLayoutParams) {
                params.setMargins(l, t, r, b)
                view.layoutParams = params
            }
        }
    }

    fun setViewGravity(viewId: Int, gravity: Int) {
        runOnUiThread {
            val view = viewMap[viewId]

            val androidGravity = when(gravity) {
                1 -> Gravity.CENTER
                2 -> Gravity.LEFT or Gravity.CENTER_VERTICAL
                3 -> Gravity.RIGHT or Gravity.CENTER_VERTICAL
                4 -> Gravity.TOP or Gravity.CENTER_HORIZONTAL
                5 -> Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
                6 -> Gravity.LEFT or Gravity.TOP
                7 -> Gravity.RIGHT or Gravity.TOP
                8 -> Gravity.LEFT or Gravity.BOTTOM
                9 -> Gravity.RIGHT or Gravity.BOTTOM
                else -> Gravity.NO_GRAVITY
            }

            when (view) {
                is TextView -> view.gravity = androidGravity
                is Button -> view.gravity = androidGravity
                is EditText -> view.gravity = androidGravity
                is LinearLayout -> view.gravity = androidGravity
            }
        }
    }

    fun setViewElevation(viewId: Int, elevation: Int) {
        runOnUiThread {
            viewMap[viewId]?.elevation = dpToPx(elevation).toFloat()
        }
    }

    fun setViewCornerRadius(viewId: Int, radius: Int) {
        runOnUiThread {
            val view = viewMap[viewId]
            val radiusPx = dpToPx(radius).toFloat()

            // For CardView, update the actual card
            val cardView = viewMap[viewId + 2000000]
            if (cardView is CardView) {
                cardView.radius = radiusPx
            } else if (view != null) {
                // For other views, use a rounded background
                val drawable = GradientDrawable()
                drawable.cornerRadius = radiusPx
                drawable.setColor(Color.TRANSPARENT)
                view.background = drawable
            }
        }
    }

    fun setViewBorder(viewId: Int, width: Int, color: Int) {
        runOnUiThread {
            val view = viewMap[viewId] ?: return@runOnUiThread
            val widthPx = dpToPx(width)

            val drawable = GradientDrawable()
            drawable.setStroke(widthPx, color)

            // Preserve existing background color if any
            if (view.background is ColorDrawable) {
                val bgColor = (view.background as ColorDrawable).color
                drawable.setColor(bgColor)
            }

            view.background = drawable
        }
    }

    fun httpGet(url: String, callbackId: Int, headersJson: String) {
        Thread {
            try {
                val connection = URL(url).openConnection() as HttpURLConnection
                connection.requestMethod = "GET"
                connection.connectTimeout = 15000
                connection.readTimeout = 15000

                // Parse and add headers
                if (headersJson.isNotEmpty()) {
                    parseAndAddHeaders(connection, headersJson)
                }

                val statusCode = connection.responseCode
                val response = if (statusCode in 200..299) {
                    connection.inputStream.bufferedReader().use { it.readText() }
                } else {
                    connection.errorStream?.bufferedReader()?.use { it.readText() } ?: ""
                }

                connection.disconnect()
                onHttpResponse(callbackId, statusCode in 200..299, response, statusCode)

            } catch (e: Exception) {
                Log.e(TAG, "HTTP GET error: ${e.message}", e)
                onHttpResponse(callbackId, false, "Error: ${e.message}", 0)
            }
        }.start()
    }

    fun httpPost(url: String, body: String, callbackId: Int, headersJson: String) {
        Thread {
            try {
                val connection = URL(url).openConnection() as HttpURLConnection
                connection.requestMethod = "POST"
                connection.connectTimeout = 15000
                connection.readTimeout = 15000
                connection.doOutput = true

                // Set default content type if not provided
                var hasContentType = false
                if (headersJson.isNotEmpty()) {
                    hasContentType = parseAndAddHeaders(connection, headersJson)
                }
                if (!hasContentType) {
                    connection.setRequestProperty("Content-Type", "application/json")
                }

                // Write body
                connection.outputStream.use { os ->
                    os.write(body.toByteArray())
                    os.flush()
                }

                val statusCode = connection.responseCode
                val response = if (statusCode in 200..299) {
                    connection.inputStream.bufferedReader().use { it.readText() }
                } else {
                    connection.errorStream?.bufferedReader()?.use { it.readText() } ?: ""
                }

                connection.disconnect()
                onHttpResponse(callbackId, statusCode in 200..299, response, statusCode)

            } catch (e: Exception) {
                Log.e(TAG, "HTTP POST error: ${e.message}", e)
                onHttpResponse(callbackId, false, "Error: ${e.message}", 0)
            }
        }.start()
    }

    fun httpPut(url: String, body: String, callbackId: Int, headersJson: String) {
        Thread {
            try {
                val connection = URL(url).openConnection() as HttpURLConnection
                connection.requestMethod = "PUT"
                connection.connectTimeout = 15000
                connection.readTimeout = 15000
                connection.doOutput = true

                var hasContentType = false
                if (headersJson.isNotEmpty()) {
                    hasContentType = parseAndAddHeaders(connection, headersJson)
                }
                if (!hasContentType) {
                    connection.setRequestProperty("Content-Type", "application/json")
                }

                connection.outputStream.use { os ->
                    os.write(body.toByteArray())
                    os.flush()
                }

                val statusCode = connection.responseCode
                val response = if (statusCode in 200..299) {
                    connection.inputStream.bufferedReader().use { it.readText() }
                } else {
                    connection.errorStream?.bufferedReader()?.use { it.readText() } ?: ""
                }

                connection.disconnect()
                onHttpResponse(callbackId, statusCode in 200..299, response, statusCode)

            } catch (e: Exception) {
                Log.e(TAG, "HTTP PUT error: ${e.message}", e)
                onHttpResponse(callbackId, false, "Error: ${e.message}", 0)
            }
        }.start()
    }

    fun httpDelete(url: String, callbackId: Int, headersJson: String) {
        Thread {
            try {
                val connection = URL(url).openConnection() as HttpURLConnection
                connection.requestMethod = "DELETE"
                connection.connectTimeout = 15000
                connection.readTimeout = 15000

                if (headersJson.isNotEmpty()) {
                    parseAndAddHeaders(connection, headersJson)
                }

                val statusCode = connection.responseCode
                val response = if (statusCode in 200..299) {
                    connection.inputStream.bufferedReader().use { it.readText() }
                } else {
                    connection.errorStream?.bufferedReader()?.use { it.readText() } ?: ""
                }

                connection.disconnect()
                onHttpResponse(callbackId, statusCode in 200..299, response, statusCode)

            } catch (e: Exception) {
                Log.e(TAG, "HTTP DELETE error: ${e.message}", e)
                onHttpResponse(callbackId, false, "Error: ${e.message}", 0)
            }
        }.start()
    }

    private fun parseAndAddHeaders(connection: HttpURLConnection, headersJson: String): Boolean {
        var hasContentType = false
        try {
            // Simple JSON parsing for headers (format: {"key":"value","key2":"value2"})
            val cleanJson = headersJson.trim().removeSurrounding("{", "}")
            if (cleanJson.isEmpty()) return false

            cleanJson.split(",").forEach { pair ->
                val parts = pair.split(":")
                if (parts.size == 2) {
                    val key = parts[0].trim().removeSurrounding("\"")
                    val value = parts[1].trim().removeSurrounding("\"")
                    connection.setRequestProperty(key, value)
                    if (key.equals("Content-Type", ignoreCase = true)) {
                        hasContentType = true
                    }
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing headers: ${e.message}")
        }
        return hasContentType
    }

    fun clearScreen(screenId: Int) {
        runOnUiThread {
            Log.d(TAG, "Clearing screen: $screenId")
            val screen = screenMap[screenId]
            if (screen == null) {
                Log.e(TAG, "Screen not found: $screenId")
                return@runOnUiThread
            }

            // Remove all child views from the screen's container
            screen.container.removeAllViews()
            Log.d(TAG, "Screen $screenId cleared successfully")
        }
    }


    override fun onDestroy() {
        super.onDestroy()
        DropletVM().cleanup()
    }

    private external fun registerVM()
    private external fun onButtonClick(callbackId: Int)
    private external fun onHttpResponse(callbackId: Int, success: Boolean, response: String, statusCode: Int)
}

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