package co.asde.towner

import android.os.Bundle
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.appcompat.app.AppCompatActivity

class MainActivity : AppCompatActivity(), SurfaceHolder.Callback {

    private lateinit var surfaceView: SurfaceView

    @Volatile private var running = false
    private var renderThread: Thread? = null

    external fun nativeInit()
    external fun nativeSurfaceCreated(surface: android.view.Surface)
    external fun nativeSurfaceChanged(w: Int, h: Int)
    external fun nativeDrawFrame()
    external fun nativeTap(x: Float, y: Float)
    external fun nativeSurfaceDestroyed()

    companion object {
        init { System.loadLibrary("towner") }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        surfaceView = SurfaceView(this)
        surfaceView.holder.addCallback(this)
        setContentView(surfaceView)
        nativeInit()
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        nativeSurfaceCreated(holder.surface)
        running = true
        renderThread = Thread {
            while (running) {
                nativeDrawFrame()
                try { Thread.sleep(16) } catch (_: InterruptedException) { break }
            }
        }.also { it.start() }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        nativeSurfaceChanged(width, height)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        running = false
        renderThread?.join(500)
        renderThread = null
        nativeSurfaceDestroyed()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (event.action == MotionEvent.ACTION_DOWN) {
            nativeTap(event.x, event.y)
        }
        return true
    }
}