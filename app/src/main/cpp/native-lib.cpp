#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <atomic>
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

// Указатель на eglSwapBuffersWithDamageKHR, если поддерживается
static PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC eglSwapBuffersWithDamageKHR = nullptr;

extern "C" {

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeInit(JNIEnv*, jobject) {
    LOGI("nativeInit called");
    grid.place(0, 0, 0, glm::vec3(0.9f, 0.5f, 0.3f));
    grid.place(1, 0, 0, glm::vec3(0.3f, 0.7f, 0.9f));
    grid.place(1, 1, 0, glm::vec3(0.5f, 0.9f, 0.4f));
    LOGI("Grid initialized with %d blocks", grid.count());
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceCreated(JNIEnv* env, jobject, jobject surface) {
    g_rendering = false;

    window = ANativeWindow_fromSurface(env, surface);
    if (!window) { LOGI("ERROR: no window"); return; }

    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) { LOGI("ERROR: no display"); return; }

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
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
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

    // ⭐ Атрибуты поверхности: EGL_BUFFER_DESTROYED вместо PRESERVED
    const EGLint surfAttribs[] = {
            EGL_SWAP_BEHAVIOR, EGL_BUFFER_DESTROYED,
            EGL_NONE
    };
    eglSurface = eglCreateWindowSurface(eglDisplay, config, window, surfAttribs);
    if (eglSurface == EGL_NO_SURFACE) {
        // Попробуем без атрибутов
        LOGI("WARN: surfAttribs failed, trying nullptr");
        eglSurface = eglCreateWindowSurface(eglDisplay, config, window, nullptr);
    }
    if (eglSurface == EGL_NO_SURFACE) {
        LOGI("ERROR: eglCreateWindowSurface failed");
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        LOGI("ERROR: eglCreateContext failed");
        eglDestroySurface(eglDisplay, eglSurface);
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
        LOGI("ERROR: eglMakeCurrent failed: %d", eglGetError());
        return;
    }

    // ⭐ Отключаем VSync — может обойти createNativeSync
    eglSwapInterval(eglDisplay, 0);

    // ⭐ Пытаемся получить указатель на eglSwapBuffersWithDamageKHR
    eglSwapBuffersWithDamageKHR = (PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC)
            eglGetProcAddress("eglSwapBuffersWithDamageKHR");
    if (eglSwapBuffersWithDamageKHR) {
        LOGI("eglSwapBuffersWithDamageKHR available");
    } else {
        LOGI("eglSwapBuffersWithDamageKHR NOT available");
    }

    renderer.init();

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        LOGI("ERROR: glGetError after init: %d", err);
        return;
    }

    g_rendering = true;
    LOGI("EGL initialized, rendering enabled");
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    if (!g_rendering) return;
    renderer.resize(w, h);
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeDrawFrame(JNIEnv*, jobject) {
    if (!g_rendering) return;
    if (eglDisplay == EGL_NO_DISPLAY) return;
    if (eglSurface == EGL_NO_SURFACE) return;

    renderer.draw(grid);

    // ⭐ Используем WithDamage, если доступен — иначе обычный swap
    if (eglSwapBuffersWithDamageKHR) {
        eglSwapBuffersWithDamageKHR(eglDisplay, eglSurface, nullptr, 0);
    } else {
        eglSwapBuffers(eglDisplay, eglSurface);
    }
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeTap(JNIEnv*, jobject, jfloat x, jfloat y) {
    if (!g_rendering) return;
    int cx, cy, cz;
    if (renderer.pickCell(grid, x, y, cx, cy, cz)) {
        if (grid.isOccupied(cx, cy, cz)) {
            grid.remove(cx, cy, cz);
        } else {
            grid.place(cx, cy, cz, glm::vec3(0.7f, 0.8f, 0.9f));
        }
        LOGI("Tap -> (%d, %d, %d), blocks: %d", cx, cy, cz, grid.count());
    }
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    g_rendering = false;

    renderer.destroy();

    if (eglDisplay != EGL_NO_DISPLAY) {
        eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eglContext != EGL_NO_CONTEXT) eglDestroyContext(eglDisplay, eglContext);
        if (eglSurface != EGL_NO_SURFACE) eglDestroySurface(eglDisplay, eglSurface);
        eglTerminate(eglDisplay);
    }
    eglDisplay = EGL_NO_DISPLAY;
    eglContext = EGL_NO_CONTEXT;
    eglSurface = EGL_NO_SURFACE;
    eglSwapBuffersWithDamageKHR = nullptr;

    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }
    LOGI("EGL destroyed");
}

} // extern "C"