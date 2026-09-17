#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <atomic>
#include <mutex>
#include <glm/glm.hpp>
#include "Renderer.h"
#include "Grid.h"

#define LOG_TAG "Towner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static Renderer renderer;
static Grid grid;
static ANativeWindow* window = nullptr;
static EGLDisplay eglDisplay = EGL_NO_DISPLAY;
static EGLSurface eglSurface = EGL_NO_SURFACE;
static EGLContext eglContext = EGL_NO_CONTEXT;

static std::atomic<bool> g_rendering{false};
static std::mutex g_mutex;

static glm::vec3 g_currentColor = glm::vec3(0.92f, 0.45f, 0.35f);

static PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC eglSwapBuffersWithDamageKHR = nullptr;

extern "C" {

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeInit(JNIEnv*, jobject) {
    LOGI("nativeInit called");
    grid.place(0, 0, 0, glm::vec3(0.92f, 0.45f, 0.35f));
    grid.place(1, 0, 0, glm::vec3(0.35f, 0.55f, 0.85f));
    grid.place(1, 1, 0, glm::vec3(0.45f, 0.75f, 0.45f));
    LOGI("Grid initialized with %d blocks", grid.count());
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSetColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_currentColor = glm::vec3(r, g, b);
    LOGI("Color set to %.2f %.2f %.2f", r, g, b);
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceCreated(JNIEnv* env, jobject, jobject surface) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_rendering = false;

    if (eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay, eglContext);
            eglContext = EGL_NO_CONTEXT;
        }
        if (eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(eglDisplay, eglSurface);
            eglSurface = EGL_NO_SURFACE;
        }
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
    }
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }

    window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGI("ERROR: no window");
        return;
    }

    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        LOGI("ERROR: no display");
        return;
    }

    EGLint major, minor;
    if (!eglInitialize(eglDisplay, &major, &minor)) {
        LOGI("ERROR: eglInitialize failed");
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }
    LOGI("EGL version %d.%d", major, minor);

    const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(eglDisplay, attribs, &config, 1, &numConfigs) || numConfigs == 0) {
        LOGI("ERROR: no EGL config");
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    const EGLint surfAttribs[] = {
            EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED,
            EGL_NONE
    };

    eglSurface = eglCreateWindowSurface(eglDisplay, config, window, surfAttribs);
    if (eglSurface == EGL_NO_SURFACE) {
        eglSurface = eglCreateWindowSurface(eglDisplay, config, window, nullptr);
    }
    if (eglSurface == EGL_NO_SURFACE) {
        LOGI("ERROR: eglCreateWindowSurface failed");
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    const EGLint ctxAttribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
    };

    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        LOGI("ERROR: eglCreateContext failed");
        eglDestroySurface(eglDisplay, eglSurface);
        eglSurface = EGL_NO_SURFACE;
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
        LOGI("ERROR: eglMakeCurrent failed: %d", eglGetError());
        eglDestroyContext(eglDisplay, eglContext);
        eglDestroySurface(eglDisplay, eglSurface);
        eglTerminate(eglDisplay);
        eglContext = EGL_NO_CONTEXT;
        eglSurface = EGL_NO_SURFACE;
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    eglSwapInterval(eglDisplay, 0);

    eglSwapBuffersWithDamageKHR = (PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC)
            eglGetProcAddress("eglSwapBuffersWithDamageKHR");

    renderer.init();

    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    g_rendering = true;
    LOGI("EGL initialized, rendering enabled");
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_rendering) return;
    if (eglDisplay == EGL_NO_DISPLAY || eglContext == EGL_NO_CONTEXT) return;

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
        LOGI("eglMakeCurrent failed in surfaceChanged: %d", eglGetError());
        return;
    }

    renderer.resize(w, h);

    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeLongPress(JNIEnv*, jobject, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_rendering) return;

    int cx, cy, cz;
    if (renderer.pickCell(grid, x, y, cx, cy, cz)) {
        // При долгом нажатии всегда удаляем блок под пальцем
        // (берём тот, в который попали, а не тот, что сверху)
        // Для этого чуть меняем логику — удаляем hit-блок

        // Простой вариант: удаляем то, что pickCell вернул как "место для постановки"
        // Но лучше удалять именно нажатый блок.
        // Поэтому делаем небольшую доработку:

        // Сначала пробуем найти блок, в который реально попали
        // (для простоты пока удаляем на позиции cy-1, если она занята)
        if (cy > 0 && grid.isOccupied(cx, cy - 1, cz)) {
            grid.remove(cx, cy - 1, cz);
            LOGI("LongPress remove (%d, %d, %d)", cx, cy - 1, cz);
        } else if (grid.isOccupied(cx, cy, cz)) {
            grid.remove(cx, cy, cz);
            LOGI("LongPress remove (%d, %d, %d)", cx, cy, cz);
        }
    }
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeDrawFrame(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_rendering) return;
    if (eglDisplay == EGL_NO_DISPLAY ||
        eglSurface == EGL_NO_SURFACE ||
        eglContext == EGL_NO_CONTEXT) {
        return;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
        LOGI("eglMakeCurrent failed in drawFrame: %d", eglGetError());
        return;
    }

    renderer.draw(grid);

    if (eglSwapBuffersWithDamageKHR) {
        eglSwapBuffersWithDamageKHR(eglDisplay, eglSurface, nullptr, 0);
    } else {
        eglSwapBuffers(eglDisplay, eglSurface);
    }
}



JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeTap(JNIEnv*, jobject, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_rendering) return;

    int cx, cy, cz;
    if (renderer.pickCell(grid, x, y, cx, cy, cz)) {

        if (grid.isOccupied(cx, cy, cz)) {
            grid.remove(cx, cy, cz);
        } else {
            grid.place(cx, cy, cz, g_currentColor);
        }
        LOGI("Tap -> (%d, %d, %d), blocks: %d", cx, cy, cz, grid.count());
    }
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeOrbit(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    std::lock_guard<std::mutex> lock(g_mutex);
    renderer.orbit(dx, dy);
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeZoom(JNIEnv*, jobject, jfloat scale) {
    std::lock_guard<std::mutex> lock(g_mutex);
    renderer.zoom(scale);
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_rendering = false;

    renderer.destroy();

    if (eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (eglContext != EGL_NO_CONTEXT) {
            eglDestroyContext(eglDisplay, eglContext);
            eglContext = EGL_NO_CONTEXT;
        }
        if (eglSurface != EGL_NO_SURFACE) {
            eglDestroySurface(eglDisplay, eglSurface);
            eglSurface = EGL_NO_SURFACE;
        }
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
    }

    eglSwapBuffersWithDamageKHR = nullptr;

    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }

    LOGI("EGL destroyed");
}

} // extern "C"