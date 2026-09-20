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
import android.widget.HorizontalScrollView
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import kotlin.math.sqrt

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
    private lateinit var rootLayout: FrameLayout
    private lateinit var paletteScroll: HorizontalScrollView

    private var currentWorldName = "world1"
    private var worldCounter = 1
    private var currentMaterial = 0

    @Volatile private var running = false
    private var renderThread: Thread? = null
    private val lock = Object()

    private var selectedColor = floatArrayOf(0.92f, 0.45f, 0.35f)

    private var lastX = 0f
    private var lastY = 0f
    private var isDragging = false
    private lateinit var scaleDetector: ScaleGestureDetector

    private val longPressHandler = Handler(Looper.getMainLooper())
    private var longPressTriggered = false
    private var downX = 0f
    private var downY = 0f
    private val LONG_PRESS_TIME = 600L
    private val MOVE_THRESHOLD = 22f

    private var lastPanX = 0f
    private var lastPanY = 0f
    private var isPanning = false
    private lateinit var materialLabel: android.widget.TextView

    private val autoSaveHandler = Handler(Looper.getMainLooper())
    private val AUTO_SAVE_INTERVAL = 30_000L
    private var isDirty = false
    private val autoSaveRunnable = object : Runnable {
        override fun run() {
            if (isDirty) {
                nativeSaveWorld(currentWorldName)
                isDirty = false
            }
            autoSaveHandler.postDelayed(this, AUTO_SAVE_INTERVAL)
        }
    }

    // ===== External =====
    external fun nativeSetWorldsDir(path: String)
    external fun nativeSaveWorld(name: String): Boolean
    external fun nativeLoadWorld(name: String): Boolean
    external fun nativeNewWorld()
    external fun nativeDeleteWorld(name: String): Boolean
    external fun nativeRenameWorld(oldName: String, newName: String): Boolean
    external fun nativeUndo()
    external fun nativeRedo()
    external fun nativePan(dx: Float, dy: Float)
    external fun nativeInit()
    external fun nativeSurfaceCreated(surface: android.view.Surface)
    external fun nativeSurfaceChanged(w: Int, h: Int)
    external fun nativeDrawFrame()
    external fun nativeTap(x: Float, y: Float)
    external fun nativeLongPress(x: Float, y: Float)
    external fun nativeSurfaceDestroyed()
    external fun nativeSetColor(r: Float, g: Float, b: Float)
    external fun nativeSetMaterial(mat: Int)
    external fun nativeOrbit(dx: Float, dy: Float)
    external fun nativeZoom(scale: Float)

    companion object {
        init { System.loadLibrary("towner") }
    }

    data class PaletteItem(
        val isMaterial: Boolean,
        val materialId: Int = 0,
        val color: FloatArray = floatArrayOf(1f, 1f, 1f),
        val name: String = ""
    )

    private val paletteItems = listOf(
        // Чистые цвета
        PaletteItem(false, 0, floatArrayOf(0.92f, 0.45f, 0.35f), "Терракота"),
        PaletteItem(false, 0, floatArrayOf(0.95f, 0.75f, 0.40f), "Жёлтый"),
        PaletteItem(false, 0, floatArrayOf(0.45f, 0.75f, 0.45f), "Зелёный"),
        PaletteItem(false, 0, floatArrayOf(0.35f, 0.55f, 0.85f), "Синий"),
        PaletteItem(false, 0, floatArrayOf(0.75f, 0.45f, 0.85f), "Сиреневый"),
        PaletteItem(false, 0, floatArrayOf(0.95f, 0.95f, 0.95f), "Белый"),
        PaletteItem(false, 0, floatArrayOf(0.25f, 0.25f, 0.28f), "Тёмно-серый"),
        PaletteItem(false, 0, floatArrayOf(0.55f, 0.35f, 0.25f), "Коричневый"),
        PaletteItem(false, 0, floatArrayOf(0.85f, 0.15f, 0.15f), "Красный"),
        PaletteItem(false, 0, floatArrayOf(0.50f, 0.10f, 0.15f), "Бордовый"),
        PaletteItem(false, 0, floatArrayOf(0.95f, 0.55f, 0.15f), "Оранжевый"),
        PaletteItem(false, 0, floatArrayOf(0.45f, 0.20f, 0.75f), "Фиолетовый"),
        PaletteItem(false, 0, floatArrayOf(0.55f, 0.55f, 0.55f), "Серый"),
        PaletteItem(false, 0, floatArrayOf(0.95f, 0.55f, 0.70f), "Розовый"),
        PaletteItem(false, 0, floatArrayOf(0.05f, 0.05f, 0.05f), "Чёрный"),

        // Материалы
        PaletteItem(true, 1,  floatArrayOf(0.75f, 0.35f, 0.25f), "Кирпич"),
        PaletteItem(true, 2,  floatArrayOf(0.55f, 0.55f, 0.58f), "Камень"),
        PaletteItem(true, 3,  floatArrayOf(0.70f, 0.85f, 0.95f), "Стекло"),
        PaletteItem(true, 4,  floatArrayOf(0.65f, 0.42f, 0.25f), "Дерево"),
        PaletteItem(true, 5,  floatArrayOf(0.70f, 0.72f, 0.75f), "Металл"),
        PaletteItem(true, 6,  floatArrayOf(0.72f, 0.50f, 0.30f), "Доски"),
        // PaletteItem(true, 7,  floatArrayOf(0.92f, 0.92f, 0.95f), "Моноблок"),
        PaletteItem(true, 8,  floatArrayOf(0.85f, 0.85f, 0.88f), "Плитка"),
        PaletteItem(true, 9,  floatArrayOf(0.45f, 0.45f, 0.48f), "Гранит"),
        PaletteItem(true, 10, floatArrayOf(0.90f, 0.82f, 0.55f), "Песок"),
        PaletteItem(true, 11, floatArrayOf(0.88f, 0.85f, 0.78f), "Известняк"),
        PaletteItem(true, 12, floatArrayOf(0.60f, 0.45f, 0.55f), "Ткань"),
        PaletteItem(true, 13, floatArrayOf(0.95f, 0.80f, 0.25f), "Золото"),
        // PaletteItem(true, 14, floatArrayOf(0.92f, 0.92f, 0.95f), "Наноблок 2.0"),
        // PaletteItem(true, 15, floatArrayOf(0.92f, 0.92f, 0.95f), "Чёрный наноблок"),
        PaletteItem(true, 16, floatArrayOf(0.92f, 0.92f, 0.95f), "Наноблок")
    )

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        rootLayout = FrameLayout(this)
        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(this)
        rootLayout.addView(surfaceView)

        // ===== Палитра =====
        paletteScroll = HorizontalScrollView(this).apply {
            setBackgroundColor(Color.parseColor("#CC111111"))
            isHorizontalScrollBarEnabled = false
            overScrollMode = android.view.View.OVER_SCROLL_NEVER
        }

        val paletteLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(16, 16, 16, 16)
        }

        materialLabel = android.widget.TextView(this).apply {
            text = "Терракота"
            setTextColor(Color.WHITE)
            textSize = 13f
            setPadding(24, 8, 24, 4)
            setBackgroundColor(Color.parseColor("#AA000000"))
            gravity = Gravity.CENTER
        }

        rootLayout.addView(
            materialLabel,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM
            ).apply {
                bottomMargin = (70 * resources.displayMetrics.density).toInt() // чуть выше палитры
            }
        )

        paletteItems.forEach { item ->
            val btn = android.view.View(this).apply {
                val size = (52 * resources.displayMetrics.density).toInt()
                layoutParams = LinearLayout.LayoutParams(size, size).apply {
                    setMargins(6, 0, 6, 0)
                }
                setBackgroundColor(
                    Color.rgb(
                        (item.color[0] * 255).toInt(),
                        (item.color[1] * 255).toInt(),
                        (item.color[2] * 255).toInt()
                    )
                )
                elevation = 6f
                if (item.materialId == 3) alpha = 0.7f // стекло

                setOnClickListener {
                    if (item.isMaterial) {
                        currentMaterial = item.materialId
                        nativeSetMaterial(item.materialId)
                        selectedColor = item.color
                        nativeSetColor(item.color[0], item.color[1], item.color[2])
                    } else {
                        currentMaterial = 0
                        nativeSetMaterial(0)
                        selectedColor = item.color
                        nativeSetColor(item.color[0], item.color[1], item.color[2])
                    }

                    // ← ЭТА СТРОКА ОБЯЗАТЕЛЬНА
                    materialLabel.text = item.name

                    animate().scaleX(0.82f).scaleY(0.82f).setDuration(70)
                        .withEndAction {
                            animate().scaleX(1f).scaleY(1f).setDuration(70).start()
                        }.start()
                }
            }
            paletteLayout.addView(btn)
        }

        paletteScroll.addView(paletteLayout)
        rootLayout.addView(
            paletteScroll,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM
            )
        )

        // ===== Кнопки =====
        val buttonsLayout = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(16, 48, 16, 12)
        }

        fun makeButton(text: String, onClick: () -> Unit): android.widget.TextView {
            return android.widget.TextView(this).apply {
                this.text = text
                setTextColor(Color.WHITE)
                textSize = 15f
                setPadding(22, 14, 22, 14)
                setBackgroundColor(Color.parseColor("#AA000000"))
                setOnClickListener { onClick() }
                elevation = 6f
            }
        }

        fun spacer() {
            buttonsLayout.addView(android.view.View(this).apply {
                layoutParams = LinearLayout.LayoutParams(10, 1)
            })
        }

        buttonsLayout.addView(makeButton("↶") { nativeUndo() })
        spacer()
        buttonsLayout.addView(makeButton("↷") { nativeRedo() })
        spacer()
        buttonsLayout.addView(makeButton("Новый") {
            worldCounter++
            currentWorldName = "world$worldCounter"
            nativeNewWorld()
            saveLastWorldName()
            isDirty = true
            toast("Создан: $currentWorldName")
        })
        spacer()
        buttonsLayout.addView(makeButton("Сохранить") {
            if (nativeSaveWorld(currentWorldName)) {
                saveLastWorldName()
                isDirty = false
                toast("Сохранён: $currentWorldName")
            } else toast("Ошибка сохранения")
        })
        spacer()
        buttonsLayout.addView(makeButton("Миры") { showWorldsDialog() })

        rootLayout.addView(
            buttonsLayout,
            FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP or Gravity.END
            )
        )

        setContentView(rootLayout)

        scaleDetector = ScaleGestureDetector(this,
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScale(detector: ScaleGestureDetector): Boolean {
                    nativeZoom(detector.scaleFactor)
                    return true
                }
            })

        nativeInit()

        val worldsDir = File(filesDir, "worlds")
        if (!worldsDir.exists()) worldsDir.mkdirs()
        nativeSetWorldsDir(worldsDir.absolutePath)

        currentWorldName = getSharedPreferences("towner", MODE_PRIVATE)
            .getString("last_world", "world1") ?: "world1"

        if (!nativeLoadWorld(currentWorldName)) {
            nativeNewWorld()
        }

        nativeSetColor(selectedColor[0], selectedColor[1], selectedColor[2])
        nativeSetMaterial(0)
        autoSaveHandler.postDelayed(autoSaveRunnable, AUTO_SAVE_INTERVAL)
    }

    // ===== Миры =====
    private fun showWorldsDialog() {
        val worldsDir = File(filesDir, "worlds")
        val files = worldsDir.listFiles { f -> f.extension == "world" }
            ?.map { it.nameWithoutExtension }
            ?.sorted()
            ?: emptyList()

        val items = files.toTypedArray()
        if (items.isEmpty()) {
            toast("Нет сохранённых миров")
            return
        }

        android.app.AlertDialog.Builder(this)
            .setTitle("Выбери мир")
            .setItems(items) { _, which ->
                val name = items[which]
                if (nativeLoadWorld(name)) {
                    currentWorldName = name
                    saveLastWorldName()
                    toast("Загружен: $name")
                } else toast("Не удалось загрузить")
            }
            .setNeutralButton("Управление") { _, _ -> showWorldActionsDialog(items) }
            .setNegativeButton("Отмена", null)
            .show()
    }

    private fun showWorldActionsDialog(items: Array<String>) {
        android.app.AlertDialog.Builder(this)
            .setTitle("Выбери мир")
            .setItems(items) { _, which -> showWorldActionMenu(items[which]) }
            .setNegativeButton("Отмена", null)
            .show()
    }

    private fun showWorldActionMenu(name: String) {
        android.app.AlertDialog.Builder(this)
            .setTitle(name)
            .setItems(arrayOf("Переименовать", "Удалить")) { _, which ->
                when (which) {
                    0 -> showRenameDialog(name)
                    1 -> showDeleteConfirm(name)
                }
            }
            .setNegativeButton("Отмена", null)
            .show()
    }

    private fun showRenameDialog(oldName: String) {
        val input = android.widget.EditText(this).apply { setText(oldName) }
        android.app.AlertDialog.Builder(this)
            .setTitle("Новое имя")
            .setView(input)
            .setPositiveButton("OK") { _, _ ->
                val newName = input.text.toString().trim()
                if (newName.isNotEmpty() && newName != oldName) {
                    if (nativeRenameWorld(oldName, newName)) {
                        toast("Переименован: $oldName → $newName")
                        if (currentWorldName == oldName) {
                            currentWorldName = newName
                            saveLastWorldName()
                        }
                    } else toast("Ошибка переименования")
                }
            }
            .setNegativeButton("Отмена", null)
            .show()
    }

    private fun showDeleteConfirm(name: String) {
        android.app.AlertDialog.Builder(this)
            .setTitle("Удалить мир?")
            .setMessage("Мир \"$name\" будет удалён навсегда.")
            .setPositiveButton("Удалить") { _, _ ->
                if (nativeDeleteWorld(name)) {
                    toast("Удалён: $name")
                    if (currentWorldName == name) {
                        currentWorldName = "world1"
                        saveLastWorldName()
                        nativeNewWorld()
                    }
                } else toast("Ошибка удаления")
            }
            .setNegativeButton("Отмена", null)
            .show()
    }

    private fun saveLastWorldName() {
        getSharedPreferences("towner", MODE_PRIVATE)
            .edit()
            .putString("last_world", currentWorldName)
            .apply()
    }

    private fun toast(msg: String) {
        android.widget.Toast.makeText(this, msg, android.widget.Toast.LENGTH_SHORT).show()
    }

    // ===== Touch =====
    override fun onTouchEvent(event: MotionEvent): Boolean {
        // Зум всегда отдаём ScaleGestureDetector (работает на 2 пальца)
        scaleDetector.onTouchEvent(event)

        val loc = IntArray(2)
        surfaceView.getLocationOnScreen(loc)
        val touchX = event.rawX - loc[0]
        val touchY = event.rawY - loc[1]

        val paletteHeight = paletteScroll.height.toFloat()
        if (touchY > surfaceView.height - paletteHeight - 4f) {
            return false
        }

        when (event.actionMasked) {

            MotionEvent.ACTION_DOWN -> {
                if (event.pointerCount == 1) {
                    lastX = touchX
                    lastY = touchY
                    downX = touchX
                    downY = touchY
                    isDragging = true
                    longPressTriggered = false
                    isPanning = false
                    longPressHandler.postDelayed(longPressRunnable, LONG_PRESS_TIME)
                }
            }

            MotionEvent.ACTION_POINTER_DOWN -> {
                if (event.pointerCount == 3) {
                    // Начали третий палец → включаем pan
                    longPressHandler.removeCallbacks(longPressRunnable)
                    isDragging = false
                    isPanning = true

                    lastPanX = (event.getX(0) + event.getX(1) + event.getX(2)) / 3f
                    lastPanY = (event.getY(0) + event.getY(1) + event.getY(2)) / 3f
                } else if (event.pointerCount == 2) {
                    // Второй палец — отменяем long press и orbit
                    longPressHandler.removeCallbacks(longPressRunnable)
                    isDragging = false
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 3 && isPanning) {
                    // ===== 3 пальца — передвижение камеры =====
                    val midX = (event.getX(0) + event.getX(1) + event.getX(2)) / 3f
                    val midY = (event.getY(0) + event.getY(1) + event.getY(2)) / 3f

                    nativePan(midX - lastPanX, midY - lastPanY)

                    lastPanX = midX
                    lastPanY = midY
                }
                else if (event.pointerCount == 1 && isDragging && !scaleDetector.isInProgress) {
                    // ===== 1 палец — вращение =====
                    val dx = touchX - lastX
                    val dy = touchY - lastY

                    val totalDx = touchX - downX
                    val totalDy = touchY - downY
                    if (sqrt(totalDx * totalDx + totalDy * totalDy) > MOVE_THRESHOLD) {
                        longPressHandler.removeCallbacks(longPressRunnable)
                    }

                    nativeOrbit(dx, dy)
                    lastX = touchX
                    lastY = touchY
                }
            }

            MotionEvent.ACTION_POINTER_UP -> {
                if (event.pointerCount <= 3) {
                    isPanning = false
                }
                if (event.pointerCount <= 2) {
                    isDragging = false
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                longPressHandler.removeCallbacks(longPressRunnable)
                isPanning = false

                if (event.pointerCount <= 1 && isDragging && !longPressTriggered) {
                    val dx = touchX - downX
                    val dy = touchY - downY
                    if (sqrt(dx * dx + dy * dy) < MOVE_THRESHOLD) {
                        nativeTap(touchX, touchY)
                        isDirty = true
                    }
                }
                isDragging = false
            }
        }
        return true
    }

    private val longPressRunnable = Runnable {
        longPressTriggered = true
        nativeLongPress(downX, downY)
        isDirty = true
    }

    // ===== Surface =====
    override fun surfaceCreated(holder: SurfaceHolder) {
        synchronized(lock) {
            stopRenderThread()
            nativeSurfaceCreated(holder.surface)
            running = true
            renderThread = Thread {
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_DISPLAY)
                while (running) {
                    try {
                        nativeDrawFrame()
                        Thread.sleep(14)
                    } catch (_: InterruptedException) {
                        break
                    } catch (_: Exception) {}
                }
            }.also {
                it.name = "TownerRender"
                it.start()
            }
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        // Берём реальный размер View, а не то, что пришло
        val w = surfaceView.width
        val h = surfaceView.height
        if (w > 1 && h > 1) {
            nativeSurfaceChanged(w, h)
        } else {
            nativeSurfaceChanged(width, height)
        }
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

    override fun onPause() {
        super.onPause()
        if (isDirty) {
            nativeSaveWorld(currentWorldName)
            isDirty = false
        }
        autoSaveHandler.removeCallbacks(autoSaveRunnable)
    }

    override fun onResume() {
        super.onResume()
        autoSaveHandler.postDelayed(autoSaveRunnable, AUTO_SAVE_INTERVAL)
    }

    override fun onDestroy() {
        super.onDestroy()
        longPressHandler.removeCallbacks(longPressRunnable)
        autoSaveHandler.removeCallbacks(autoSaveRunnable)
        synchronized(lock) {
            stopRenderThread()
        }
    }
}