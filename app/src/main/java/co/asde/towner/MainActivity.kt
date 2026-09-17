package co.asde.towner

import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.FrameLayout
import android.widget.LinearLayout
import androidx.appcompat.app.AppCompatActivity
import kotlin.math.atan2
import kotlin.math.sqrt

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView
    private lateinit var rootLayout: FrameLayout

    @Volatile private var running = false
    private var renderThread: Thread? = null
    private val lock = Object()

    private var selectedColor = floatArrayOf(0.92f, 0.45f, 0.35f)

    // Камера
    private var lastX = 0f
    private var lastY = 0f
    private var isDragging = false
    private lateinit var scaleDetector: ScaleGestureDetector

    external fun nativeInit()
    external fun nativeSurfaceCreated(surface: android.view.Surface)
    external fun nativeSurfaceChanged(w: Int, h: Int)
    external fun nativeDrawFrame()
    external fun nativeTap(x: Float, y: Float)
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

        // Палитра
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

        // Детектор масштаба
        scaleDetector = ScaleGestureDetector(this, object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
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

        val paletteHeight = 140f
        if (event.y > surfaceView.height - paletteHeight) {
            return true // тапы по палитре игнорируем для строительства
        }

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                if (event.pointerCount == 1) {
                    lastX = event.x
                    lastY = event.y
                    isDragging = true
                }
            }

            MotionEvent.ACTION_MOVE -> {
                if (event.pointerCount == 1 && isDragging && !scaleDetector.isInProgress) {
                    val dx = event.x - lastX
                    val dy = event.y - lastY
                    nativeOrbit(dx, dy)
                    lastX = event.x
                    lastY = event.y
                }
            }

            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (event.pointerCount <= 1) {
                    // Если почти не двигали — это тап (строительство)
                    val dx = event.x - lastX
                    val dy = event.y - lastY
                    if (isDragging && sqrt(dx * dx + dy * dy) < 15f) {
                        nativeTap(event.x, event.y)
                    }
                    isDragging = false
                }
            }
        }
        return true
    }

    override fun onDestroy() {
        super.onDestroy()
        synchronized(lock) {
            stopRenderThread()
            try { nativeSurfaceDestroyed() } catch (_: Throwable) {}
        }
    }
}