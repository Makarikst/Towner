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
import kotlin.math.sqrt

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
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

    private val longPressRunnable = Runnable {
        longPressTriggered = true
        nativeLongPress(downX, downY)
    }

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

        setContentView(rootLayout)

        scaleDetector = ScaleGestureDetector(this,
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScale(detector: ScaleGestureDetector): Boolean {
                    nativeZoom(detector.scaleFactor)
                    return true
                }
            })

        nativeInit()
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

    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)

        // Получаем реальные координаты относительно SurfaceView
        val loc = IntArray(2)
        surfaceView.getLocationOnScreen(loc)
        val touchX = event.rawX - loc[0]
        val touchY = event.rawY - loc[1]

        val paletteHeight = 140f
        if (touchY > surfaceView.height - paletteHeight) {
            return true
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
                    longPressHandler.postDelayed(longPressRunnable, 450)
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 1 && isDragging && !scaleDetector.isInProgress) {
                    val dx = touchX - lastX
                    val dy = touchY - lastY

                    if (sqrt(dx * dx + dy * dy) > 14f) {
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
                    if (sqrt(dx * dx + dy * dy) < 20f) {
                        nativeTap(touchX, touchY)
                    }
                }
                isDragging = false
            }
        }
        return true
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