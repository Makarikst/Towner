package co.asde.towner

import android.graphics.Color
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.Gravity
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import kotlin.math.sqrt

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView

    private var currentWorldName = "world1"
    private var worldCounter = 1

    private fun saveLastWorldName() {
        getSharedPreferences("towner", MODE_PRIVATE)
            .edit()
            .putString("last_world", currentWorldName)
            .apply()
    }

    private fun showWorldsDialog() {
        val worldsDir = File(filesDir, "worlds")
        val files = worldsDir.listFiles { f -> f.extension == "world" }
            ?.map { it.nameWithoutExtension }
            ?.sorted()
            ?: emptyList()

        val items = files.toTypedArray()

        if (items.isEmpty()) {
            android.widget.Toast.makeText(this, "Нет сохранённых миров", android.widget.Toast.LENGTH_SHORT).show()
            return
        }

        android.app.AlertDialog.Builder(this)
            .setTitle("Выбери мир")
            .setItems(items) { _, which ->
                val name = items[which]
                if (nativeLoadWorld(name)) {
                    currentWorldName = name
                    saveLastWorldName()
                    android.widget.Toast.makeText(this, "Загружен: $name", android.widget.Toast.LENGTH_SHORT).show()
                } else {
                    android.widget.Toast.makeText(this, "Не удалось загрузить", android.widget.Toast.LENGTH_SHORT).show()
                }
            }
            .setNegativeButton("Отмена", null)
            .show()
    }
    private lateinit var rootLayout: FrameLayout

    @Volatile private var running = false
    private var renderThread: Thread? = null
    private val lock = Object()

    private var selectedColor = floatArrayOf(0.92f, 0.45f, 0.35f)

    // Камера + жесты
    private var lastX = 0f
    private var lastY = 0f
    private var isDragging = false
    private lateinit var scaleDetector: ScaleGestureDetector

    // Долгое нажатие
    private val longPressHandler = Handler(Looper.getMainLooper())
    private var longPressTriggered = false
    private var downX = 0f
    private var downY = 0f

    // В начале класса
    private val LONG_PRESS_TIME = 600L
    private val MOVE_THRESHOLD = 22f

    private val longPressRunnable = Runnable {
        longPressTriggered = true
        nativeLongPress(downX, downY)
    }     // было 12-14


    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)

        val loc = IntArray(2)
        surfaceView.getLocationOnScreen(loc)
        val touchX = event.rawX - loc[0]
        val touchY = event.rawY - loc[1]

        val paletteHeight = 140f
        if (touchY > surfaceView.height - paletteHeight) return true

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (event.pointerCount == 1) {
                    lastX = touchX
                    lastY = touchY
                    downX = touchX
                    downY = touchY
                    isDragging = true
                    longPressTriggered = false

                    longPressHandler.postDelayed(longPressRunnable, LONG_PRESS_TIME)
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 1 && isDragging && !scaleDetector.isInProgress) {
                    val dx = touchX - lastX
                    val dy = touchY - lastY
                    val totalDx = touchX - downX
                    val totalDy = touchY - downY

                    // Если сдвинули палец больше порога — отменяем long press
                    if (sqrt(totalDx * totalDx + totalDy * totalDy) > MOVE_THRESHOLD) {
                        longPressHandler.removeCallbacks(longPressRunnable)
                    }

                    nativeOrbit(dx, dy)
                    lastX = touchX
                    lastY = touchY
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                longPressHandler.removeCallbacks(longPressRunnable)

                if (event.pointerCount <= 1 && isDragging && !longPressTriggered) {
                    val dx = touchX - downX
                    val dy = touchY - downY
                    if (sqrt(dx * dx + dy * dy) < MOVE_THRESHOLD) {
                        nativeTap(touchX, touchY)
                    }
                }
                isDragging = false
            }
        }
        return true
    }

    external fun nativeSetWorldsDir(path: String)

    external fun nativeSaveWorld(name: String): Boolean
    external fun nativeLoadWorld(name: String): Boolean
    external fun nativeNewWorld()
    external fun nativeInit()
    external fun nativeSurfaceCreated(surface: android.view.Surface)
    external fun nativeSurfaceChanged(w: Int, h: Int)
    external fun nativeDrawFrame()
    external fun nativeTap(x: Float, y: Float)           // короткий тап = поставить
    external fun nativeLongPress(x: Float, y: Float)     // долгое = удалить
    external fun nativeSurfaceDestroyed()
    external fun nativeSetColor(r: Float, g: Float, b: Float)
    external fun nativeOrbit(dx: Float, dy: Float)
    external fun nativeZoom(scale: Float)

    companion object {
        init { System.loadLibrary("towner") }
    }

    private val palette = listOf(
        floatArrayOf(0.92f, 0.45f, 0.35f),
        floatArrayOf(0.95f, 0.75f, 0.40f),
        floatArrayOf(0.45f, 0.75f, 0.45f),
        floatArrayOf(0.35f, 0.55f, 0.85f),
        floatArrayOf(0.75f, 0.45f, 0.85f),
        floatArrayOf(0.95f, 0.95f, 0.95f),
        floatArrayOf(0.25f, 0.25f, 0.28f),
        floatArrayOf(0.55f, 0.35f, 0.25f)
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        rootLayout = FrameLayout(this)

        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(this)
        rootLayout.addView(surfaceView)

        // ===== Палитра внизу =====
        val paletteLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER
            setBackgroundColor(Color.parseColor("#CC111111"))
            setPadding(12, 18, 12, 18)
        }

        palette.forEach { color ->
            val btn = android.view.View(this).apply {
                layoutParams = LinearLayout.LayoutParams(0, 88, 1f).apply {
                    setMargins(6, 0, 6, 0)
                }
                setBackgroundColor(
                    Color.rgb(
                        (color[0] * 255).toInt(),
                        (color[1] * 255).toInt(),
                        (color[2] * 255).toInt()
                    )
                )
                elevation = 8f
                setOnClickListener {
                    selectedColor = color
                    nativeSetColor(color[0], color[1], color[2])
                    animate().scaleX(0.82f).scaleY(0.82f).setDuration(70)
                        .withEndAction {
                            animate().scaleX(1f).scaleY(1f).setDuration(70).start()
                        }.start()
                }
            }
            paletteLayout.addView(btn)
        }

        rootLayout.addView(
            paletteLayout,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM
            )
        )

        // ===== Кнопки миров (сверху справа) =====
        val buttonsLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(16, 48, 16, 12)
        }

        fun makeButton(text: String, onClick: () -> Unit): android.widget.TextView {
            return android.widget.TextView(this).apply {
                this.text = text
                setTextColor(Color.WHITE)
                textSize = 14f
                setPadding(28, 16, 28, 16)
                setBackgroundColor(Color.parseColor("#AA000000"))
                setOnClickListener { onClick() }
                elevation = 6f
            }
        }

// Новый мир с уникальным именем
        buttonsLayout.addView(makeButton("Новый") {
            worldCounter++
            currentWorldName = "world$worldCounter"
            nativeNewWorld()
            saveLastWorldName()
            android.widget.Toast.makeText(this, "Создан: $currentWorldName", android.widget.Toast.LENGTH_SHORT).show()
        })

        buttonsLayout.addView(android.view.View(this).apply {
            layoutParams = LinearLayout.LayoutParams(12, 1)
        })

// Сохранить текущий мир
        buttonsLayout.addView(makeButton("Сохранить") {
            if (nativeSaveWorld(currentWorldName)) {
                saveLastWorldName()
                android.widget.Toast.makeText(this, "Сохранён: $currentWorldName", android.widget.Toast.LENGTH_SHORT).show()
            } else {
                android.widget.Toast.makeText(this, "Ошибка сохранения", android.widget.Toast.LENGTH_SHORT).show()
            }
        })

        buttonsLayout.addView(android.view.View(this).apply {
            layoutParams = LinearLayout.LayoutParams(12, 1)
        })

// Список миров
        buttonsLayout.addView(makeButton("Миры") {
            showWorldsDialog()
        })

        rootLayout.addView(
            buttonsLayout,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP or Gravity.END
            )
        )

        setContentView(rootLayout)

        // Scale detector
        scaleDetector = ScaleGestureDetector(this,
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScale(detector: ScaleGestureDetector): Boolean {
                    nativeZoom(detector.scaleFactor)
                    return true
                }
            })

        // Инициализация
        nativeInit()

        val worldsDir = File(filesDir, "worlds")
        if (!worldsDir.exists()) worldsDir.mkdirs()
        nativeSetWorldsDir(worldsDir.absolutePath)

        // Загружаем последний мир
        currentWorldName = getSharedPreferences("towner", MODE_PRIVATE)
            .getString("last_world", "world1") ?: "world1"

        if (!nativeLoadWorld(currentWorldName)) {
            nativeNewWorld()
        }

        nativeSetColor(selectedColor[0], selectedColor[1], selectedColor[2])
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        synchronized(lock) {
            stopRenderThread()
            nativeSurfaceCreated(holder.surface)
            running = true
            renderThread = Thread {
                while (running) {
                    try {
                        nativeDrawFrame()
                        Thread.sleep(16)
                    } catch (_: InterruptedException) {
                        break
                    }
                }
            }.also { it.start() }
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        nativeSurfaceChanged(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        synchronized(lock) {
            stopRenderThread()
            nativeSurfaceDestroyed()
        }
    }

    private fun stopRenderThread() {
        running = false
        renderThread?.let {
            it.interrupt()
            try { it.join(1000) } catch (_: InterruptedException) {}
        }
        renderThread = null
    }


    override fun onDestroy() {
        super.onDestroy()
        longPressHandler.removeCallbacks(longPressRunnable)
        synchronized(lock) {
            stopRenderThread()
            try { nativeSurfaceDestroyed() } catch (_: Throwable) {}
        }
    }
}