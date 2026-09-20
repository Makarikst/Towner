#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <atomic>
#include <mutex>
#include <string>
#include <cstdio>
#include <glm/glm.hpp>

#include "Renderer.h"
#include "Grid.h"

#define LOG_TAG "Towner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static Renderer renderer;
static Grid grid;

static ANativeWindow* window = nullptr;
static EGLDisplay eglDisplay = EGL_NO_DISPLAY;
static EGLSurface eglSurface = EGL_NO_SURFACE;
static EGLContext eglContext = EGL_NO_CONTEXT;

static std::atomic<bool> g_rendering{false};
static std::atomic<bool> g_rendererInited{false};
static std::mutex g_mutex;

static std::atomic<int> g_pendingWidth{-1};
static std::atomic<int> g_pendingHeight{-1};
static std::atomic<bool> g_hasPendingResize{false};

static glm::vec3 g_currentColor = glm::vec3(0.92f, 0.45f, 0.35f);
static uint8_t g_currentMaterial = 0;   // 0 = чистый цвет

static std::string g_worldsDir;

static PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC eglSwapBuffersWithDamageKHR = nullptr;

extern "C" {

// ===================== Инициализация =====================

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeInit(JNIEnv*, jobject) {
    LOGI("nativeInit called");
    grid.place(0, 0, 0, 0, glm::vec3(0.92f, 0.45f, 0.35f), false);
    grid.place(1, 0, 0, 0, glm::vec3(0.35f, 0.55f, 0.85f), false);
    grid.place(1, 1, 0, 0, glm::vec3(0.45f, 0.75f, 0.45f), false);
    LOGI("Grid initialized with %d blocks", grid.count());
}

// ===================== Цвет и материал =====================

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSetColor(JNIEnv*, jobject, jfloat r, jfloat g, jfloat b) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_currentColor = glm::vec3(r, g, b);
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSetMaterial(JNIEnv*, jobject, jint mat) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_currentMaterial = static_cast<uint8_t>(mat);
}

// ===================== Миры =====================

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSetWorldsDir(JNIEnv* env, jobject, jstring path) {
    const char* str = env->GetStringUTFChars(path, nullptr);
    g_worldsDir = str;
    env->ReleaseStringUTFChars(path, str);
    LOGI("Worlds dir: %s", g_worldsDir.c_str());
}

JNIEXPORT jboolean JNICALL
Java_co_asde_towner_MainActivity_nativeSaveWorld(JNIEnv* env, jobject, jstring name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const char* str = env->GetStringUTFChars(name, nullptr);
    std::string path = g_worldsDir + "/" + str + ".world";
    env->ReleaseStringUTFChars(name, str);
    bool ok = grid.save(path);
    LOGI("Save %s: %s", path.c_str(), ok ? "OK" : "FAIL");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_co_asde_towner_MainActivity_nativeLoadWorld(JNIEnv* env, jobject, jstring name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const char* str = env->GetStringUTFChars(name, nullptr);
    std::string path = g_worldsDir + "/" + str + ".world";
    env->ReleaseStringUTFChars(name, str);
    bool ok = grid.load(path);
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeNewWorld(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    grid.clear();
    grid.place(0, 0, 0, 0, glm::vec3(0.92f, 0.45f, 0.35f), false);
    grid.place(1, 0, 0, 0, glm::vec3(0.35f, 0.55f, 0.85f), false);
    grid.place(1, 1, 0, 0, glm::vec3(0.45f, 0.75f, 0.45f), false);
    LOGI("New world created");
}

JNIEXPORT jboolean JNICALL
Java_co_asde_towner_MainActivity_nativeDeleteWorld(JNIEnv* env, jobject, jstring name) {
    const char* str = env->GetStringUTFChars(name, nullptr);
    std::string path = g_worldsDir + "/" + str + ".world";
    env->ReleaseStringUTFChars(name, str);
    bool ok = (remove(path.c_str()) == 0);
    LOGI("Delete %s: %s", path.c_str(), ok ? "OK" : "FAIL");
    return ok ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_co_asde_towner_MainActivity_nativeRenameWorld(JNIEnv* env, jobject,
                                                   jstring oldName, jstring newName) {
    const char* oldStr = env->GetStringUTFChars(oldName, nullptr);
    const char* newStr = env->GetStringUTFChars(newName, nullptr);
    std::string oldPath = g_worldsDir + "/" + oldStr + ".world";
    std::string newPath = g_worldsDir + "/" + newStr + ".world";
    env->ReleaseStringUTFChars(oldName, oldStr);
    env->ReleaseStringUTFChars(newName, newStr);
    bool ok = (rename(oldPath.c_str(), newPath.c_str()) == 0);
    LOGI("Rename %s -> %s: %s", oldPath.c_str(), newPath.c_str(), ok ? "OK" : "FAIL");
    return ok ? JNI_TRUE : JNI_FALSE;
}

// ===================== Undo / Redo =====================

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeUndo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (grid.undo()) LOGI("Undo OK");
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeRedo(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (grid.redo()) LOGI("Redo OK");
}

// ===================== Surface =====================

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceCreated(JNIEnv* env, jobject, jobject surface) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_rendering = false;
    g_rendererInited = false;

    // Уничтожаем старое
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
        LOGE("ERROR: no window");
        return;
    }

    eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (eglDisplay == EGL_NO_DISPLAY) {
        LOGE("ERROR: no display");
        return;
    }

    EGLint major, minor;
    if (!eglInitialize(eglDisplay, &major, &minor)) {
        LOGE("ERROR: eglInitialize failed");
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }
    LOGI("EGL %d.%d", major, minor);

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
        LOGE("ERROR: no EGL config");
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    eglSurface = eglCreateWindowSurface(eglDisplay, config, window, nullptr);
    if (eglSurface == EGL_NO_SURFACE) {
        LOGE("ERROR: eglCreateWindowSurface failed");
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, ctxAttribs);
    if (eglContext == EGL_NO_CONTEXT) {
        LOGE("ERROR: eglCreateContext failed");
        eglDestroySurface(eglDisplay, eglSurface);
        eglSurface = EGL_NO_SURFACE;
        eglTerminate(eglDisplay);
        eglDisplay = EGL_NO_DISPLAY;
        return;
    }

    eglSwapBuffersWithDamageKHR = (PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC)
            eglGetProcAddress("eglSwapBuffersWithDamageKHR");

    g_rendering = true;
    LOGI("EGL created");
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceChanged(JNIEnv*, jobject, jint w, jint h) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pendingWidth = w;
    g_pendingHeight = h;
    g_hasPendingResize = true;
    LOGI("surfaceChanged: %d x %d", w, h);
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeSurfaceDestroyed(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_rendering = false;

    if (eglDisplay != EGL_NO_DISPLAY && eglContext != EGL_NO_CONTEXT &&
        eglSurface != EGL_NO_SURFACE && g_rendererInited) {
        eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);
        renderer.destroy();
    }

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
    g_rendererInited = false;

    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }
    LOGI("EGL destroyed");
}

// ===================== Рисование =====================

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeDrawFrame(JNIEnv*, jobject) {
    Grid snapshot;

    {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (!g_rendering) return;
        if (eglDisplay == EGL_NO_DISPLAY || eglSurface == EGL_NO_SURFACE || eglContext == EGL_NO_CONTEXT)
            return;

        // Пытаемся сделать контекст текущим
        if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
            // Не удалось — просто пропускаем кадр, не ломая состояние
            return;
        }

        // Принудительно чиним размер, если он битый
        if (g_hasPendingResize || renderer.width <= 1 || renderer.height <= 1) {
            int w = g_pendingWidth;
            int h = g_pendingHeight;
            if (w > 1 && h > 1) {
                renderer.resize(w, h);
                g_hasPendingResize = false;
            }
        }

        // Если после всего размер всё ещё плохой — не рисуем
        if (renderer.width <= 1 || renderer.height <= 1) {
            return;
        }

        if (!g_rendererInited) {
            renderer.init();
            g_rendererInited = true;
            LOGI("Renderer initialized");
        }

        // Копируем данные
        snapshot.cells = grid.cells;
    }

    // Рисуем
    renderer.draw(snapshot);

    // Swap
    if (eglSwapBuffersWithDamageKHR) {
        eglSwapBuffersWithDamageKHR(eglDisplay, eglSurface, nullptr, 0);
    } else {
        eglSwapBuffers(eglDisplay, eglSurface);
    }
}

// ===================== Ввод =====================

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeTap(JNIEnv*, jobject, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_rendering || !g_rendererInited) return;

    int cx, cy, cz;
    if (renderer.pickCell(grid, x, y, cx, cy, cz)) {
        if (grid.isOccupied(cx, cy, cz)) {
            // на всякий случай не удаляем коротким тапом
        } else {
            grid.place(cx, cy, cz, g_currentMaterial, g_currentColor, true);
            LOGI("Place at %d %d %d (mat=%d)", cx, cy, cz, g_currentMaterial);
        }
    }
}

JNIEXPORT void JNICALL
Java_co_asde_towner_MainActivity_nativeLongPress(JNIEnv*, jobject, jfloat x, jfloat y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_rendering || !g_rendererInited) return;

    int cx, cy, cz;
    if (renderer.pickBlockToDelete(grid, x, y, cx, cy, cz)) {
        grid.remove(cx, cy, cz, true);
        LOGI("Delete at %d %d %d", cx, cy, cz);
    } else {
        LOGI("LongPress: nothing under cursor");
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
Java_co_asde_towner_MainActivity_nativePan(JNIEnv*, jobject, jfloat dx, jfloat dy) {
    std::lock_guard<std::mutex> lock(g_mutex);
    renderer.pan(dx, dy);
}

} // extern "C"