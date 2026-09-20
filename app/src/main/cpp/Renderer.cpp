#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <android/log.h>
#include <cmath>
#include <algorithm>

#define LOG_TAG "Towner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* VS = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
out vec3 vWorldPos;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    gl_Position = uMVP * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
}
)";

static const char* FS = R"(#version 300 es
precision highp float;

in vec3 vNormal;
in vec3 vWorldPos;
uniform vec3 uColor;
uniform int uMaterial;
out vec4 FragColor;

float hexPattern(vec2 uv, float scale) {
    vec2 p = uv * scale;
    vec2 r = vec2(1.0, 1.7320508);
    vec2 h = r * 0.5;

    vec2 a = mod(p, r) - h;
    vec2 b = mod(p - h, r) - h;

    float da = max(abs(a.x) * 0.866025 + abs(a.y) * 0.5, abs(a.y));
    float db = max(abs(b.x) * 0.866025 + abs(b.y) * 0.5, abs(b.y));
    float d = min(da, db);

    return smoothstep(0.48, 0.42, d); // 1.0 = ячейка, 0.0 = ребро
}

float hexDist(vec2 p) {
    p = abs(p);
    return max(p.x * 0.866025 + p.y * 0.5, p.y);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float hex(vec2 p) {
    p = abs(p);
    return max(p.x * 0.866025 + p.y * 0.5, p.y);
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.35, 1.0, 0.45));
    float ndl = max(dot(N, L), 0.0);
    float lighting = 0.42 + ndl * 0.58;
    float rim = pow(1.0 - max(dot(N, vec3(0.0, 0.15, 1.0)), 0.0), 3.0) * 0.15;

    vec3 col = uColor;
    float alpha = 1.0;

    // === ИСПРАВЛЕНО: добавлено имя переменной uv ===
    vec2 uv;
    if (abs(N.y) > 0.7) uv = vWorldPos.xz;
    else if (abs(N.x) > 0.7) uv = vWorldPos.zy;
    else uv = vWorldPos.xy;

    if (uMaterial == 1) { // Кирпич
        float scaleY = 2.6;
        float scaleX = 1.7;
        float row = floor(vWorldPos.y * scaleY);
        float offset = mod(row, 2.0) * 0.5;
        float bx = fract(vWorldPos.x * scaleX + offset);
        float by = fract(vWorldPos.y * scaleY);
        float mortarX = smoothstep(0.88, 0.94, bx);
        float mortarY = smoothstep(0.84, 0.91, by);
        float mortar = max(mortarX, mortarY);
        float brickVar = noise(vec2(floor(vWorldPos.x * scaleX + offset), row)) * 0.18;
        col *= 0.88 + brickVar;
        col = mix(col, col * 0.52, mortar);
    }
    else if (uMaterial == 2) { // Камень
        float n1 = noise(uv * 4.2);
        float n2 = noise(uv * 9.5);
        col *= 0.78 + n1 * 0.28 + n2 * 0.12;
    }
    else if (uMaterial == 3) { // Стекло
        alpha = 0.32;
        // === ИСПРАВЛЕНО: добавлена закрывающая скобка ===
        float fres = pow(1.0 - abs(dot(N, vec3(0.0, 0.0, 1.0))), 2.0);
        col = mix(col, vec3(0.85, 0.92, 1.0), fres * 0.6);
        lighting = 0.7 + ndl * 0.3;
    }
    else if (uMaterial == 4) { // Дерево
        float grain = sin(vWorldPos.y * 26.0 + noise(uv * 2.8) * 5.5);
        float dark = smoothstep(-0.3, 0.6, grain);
        col *= mix(0.68, 1.05, dark);
    }
    else if (uMaterial == 5) { // Металл
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(R, normalize(vec3(0.1, 0.55, 1.0))), 0.0), 72.0);
        float fres = pow(1.0 - max(dot(N, vec3(0.0, 0.2, 1.0)), 0.0), 2.8);
        col = mix(col * 0.5, vec3(0.92), fres * 0.45);
        col += vec3(spec) * 1.15;
        lighting = 0.26 + ndl * 0.52;
    }
    else if (uMaterial == 6) { // Доски
        float id = floor(vWorldPos.x * 1.45);
        float plank = fract(vWorldPos.x * 1.45);
        float line = smoothstep(0.90, 0.96, plank);
        float grain = noise(vec2(id * 0.35, vWorldPos.y * 10.0));
        col *= mix(0.75 + grain * 0.28, 0.48, line);
    }
    else if (uMaterial == 7) { // Моноблок
        // === ИСПРАВЛЕНО: добавлена закрывающая скобка ===
        float h = abs(sin(uv.x * 11.0)) * sin(uv.y * 11.0);
        col = mix(col * 0.22, col * 1.2, step(0.68, h));
    }
    else if (uMaterial == 8) { // Плитка
        float sx = smoothstep(0.91, 0.96, fract(vWorldPos.x * 1.9));
        float sz = smoothstep(0.91, 0.96, fract(vWorldPos.z * 1.9));
        col = mix(col, col * 0.48, max(sx, sz));
    }
    else if (uMaterial == 9) { // Гранит
        col = mix(col, col * vec3(1.08, 0.95, 0.92), 0.35);
        float n1 = noise(uv * 5.0);
        float n2 = noise(uv * 13.0);
        float speck = smoothstep(0.60, 0.78, n1) * 0.65 + smoothstep(0.68, 0.83, n2) * 0.4;
        col = mix(col, col * 0.28, speck);
        col *= 0.9 + noise(uv * 19.0) * 0.15;
    }
    else if (uMaterial == 10) { // Песок
        float n = noise(uv * 14.0);
        col *= 0.82 + n * 0.28;
    }
    else if (uMaterial == 11) { // Известняк
        float n = noise(uv * 3.5);
        float veins = smoothstep(0.35, 0.65, noise(uv * 1.6 + 2.0));
        col *= 0.84 + n * 0.26;
        col = mix(col, col * 0.68, veins * 0.45);
    }
    else if (uMaterial == 12) { // Ткань
        lighting = 0.60 + ndl * 0.22;
        // === ИСПРАВЛЕНО: удалена лишняя скобка ===
        float weave = abs(sin(uv.x * 48.0)) * sin(uv.y * 48.0);
        col *= 0.88 + weave * 0.18;
    }
    else if (uMaterial == 13) { // Золото
        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(R, normalize(vec3(0.15, 0.65, 1.0))), 0.0), 56.0);
        float fres = pow(1.0 - max(dot(N, vec3(0.0, 0.25, 1.0)), 0.0), 2.2);
        vec3 goldTint = vec3(1.0, 0.78, 0.28);
        col = mix(col, goldTint, 0.4);
        col = mix(col, vec3(1.0, 0.93, 0.6), fres * 0.55);
        col += spec * vec3(1.0, 0.88, 0.45) * 1.2;
        lighting = 0.30 + ndl * 0.53;
    }
    else if (uMaterial == 14) { // Наноблок 2.0
        vec2 p = uv * 6.2;
        vec2 r = vec2(1.0, 1.73205);
        vec2 h = r * 0.5;
        vec2 a = mod(p, r) - h;
        vec2 b = mod(p - h, r) - h;
        float d = min(hex(a), hex(b));
        float cell = smoothstep(0.48, 0.40, d);
        col = mix(col * 0.18, col * 1.25, cell);
        col += (1.0 - cell) * 0.08;
    }
    // === ИСПРАВЛЕНО: перенесено внутрь main() и убрана лишняя скобка ===
    else if (uMaterial == 16) { // Наноблок — шестиугольники
    float scale = 6.0;
    vec2 p = uv * scale;

    // Осевые координаты
    float q = p.x * 0.6666667;
    float r = p.y * 0.5773503 - p.x * 0.3333333;

    // Округление до центра ячейки
    float qf = floor(q + 0.5);
    float rf = floor(r + 0.5);
    float sf = floor(-q - r + 0.5);

    float qd = abs(qf - q);
    float rd = abs(rf - r);
    float sd = abs(sf + q + r);

    if (qd > rd && qd > sd) qf = -rf - sf;
    else if (rd > sd)       rf = -qf - sf;

    // Вектор от центра в осевых координатах
    float dq = q - qf;
    float dr = r - rf;

    // === Гексагональное расстояние (не круглое!) ===
    float dist = max(abs(dq), max(abs(dr), abs(dq + dr)));

    // Тонкая граница
    float edge = smoothstep(0.42, 0.48, dist);

    // Светлая ячейка + тёмное ребро
    col = mix(col * 1.5, vec3(0.03), edge);
}
vec3 finalCol = col * lighting + rim;
    FragColor = vec4(finalCol, alpha);
}
)";



void Renderer::init() {
    if (!shader.compile(VS, FS)) {
        LOGE("Shader compile FAILED");
        return;
    }

    uMVP   = glGetUniformLocation(shader.id, "uMVP");
    uModel = glGetUniformLocation(shader.id, "uModel");
    uColor = glGetUniformLocation(shader.id, "uColor");
    uMaterial = glGetUniformLocation(shader.id, "uMaterial");

    // ============================================
    // Правильный куб — все грани против часовой стрелки (CCW)
    // при взгляде снаружи. Это важно для GL_CULL_FACE.
    // ============================================
    float cube[] = {
            // Front (+Z)
            -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,

            // Back (-Z)
            0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,

            // Left (-X)
            -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,

            // Right (+X)
            0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
            0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
            0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,

            // Bottom (-Y)
            -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
            0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,

            // Top (+Y)
            -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);

    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);          // явно указываем

    glClearColor(0.53f, 0.78f, 0.96f, 1.0f);

    updateCamera();
    LOGI("Renderer init done — solid cubes");
}

void Renderer::updateCamera() {
    // Ограничиваем pitch, чтобы не переворачивалось
    camPitch = std::max(0.15f, std::min(camPitch, 1.45f));
    camDistance = std::max(6.0f, std::min(camDistance, 45.0f));

    float x = camTarget.x + camDistance * std::cos(camPitch) * std::sin(camYaw);
    float y = camTarget.y + camDistance * std::sin(camPitch);
    float z = camTarget.z + camDistance * std::cos(camPitch) * std::cos(camYaw);

    view = glm::lookAt(
            glm::vec3(x, y, z),
            camTarget,
            glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void Renderer::orbit(float dx, float dy) {
    // Чувствительность
    camYaw   += dx * 0.007f;
    camPitch += dy * 0.005f;
    updateCamera();
}

void Renderer::zoom(float factor) {
    camDistance /= factor;   // factor > 1 = приближение
    updateCamera();
}

void Renderer::resize(int w, int h) {
    if (w <= 1 || h <= 1) {
        LOGI("resize IGNORED: %d x %d", w, h);
        return;
    }

    width = w;
    height = h;

    glViewport(0, 0, width, height);

    float aspect = (float)width / (float)height;
    proj = glm::perspective(glm::radians(42.0f), aspect, 0.1f, 200.0f);
    updateCamera();

    LOGI("resize OK → %d x %d  aspect=%.3f", width, height, aspect);
}

void Renderer::draw(const Grid& grid) {
    if (width <= 1 || height <= 1) return;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (shader.id == 0 || cubeVAO == 0) return;

    shader.use();
    glm::mat4 vp = proj * view;
    glBindVertexArray(cubeVAO);

    for (const auto& pair : grid.cells) {
        if (!pair.second.occupied) continue;

        int k = pair.first;
        int x =  k        & 0x3FF;
        int y = (k >> 10) & 0x3FF;
        int z = (k >> 20) & 0x3FF;
        if (x > 511) x -= 1024;
        if (y > 511) y -= 1024;
        if (z > 511) z -= 1024;

        glm::mat4 model = glm::translate(glm::mat4(1.0f),
                                         glm::vec3((float)x, (float)y, (float)z));
        model = glm::scale(model, glm::vec3(0.96f)); // небольшой зазор

        glm::mat4 mvp = vp * model;

        glUniformMatrix4fv(uMVP,   1, GL_FALSE, glm::value_ptr(mvp));
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(uColor, 1, glm::value_ptr(pair.second.color));
        glUniform1i(uMaterial, (int)pair.second.material);

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glBindVertexArray(0);
}

void Renderer::destroy() {
    shader.destroy();
    if (cubeVAO) { glDeleteVertexArrays(1, &cubeVAO); cubeVAO = 0; }
    if (cubeVBO) { glDeleteBuffers(1, &cubeVBO); cubeVBO = 0; }
}

bool Renderer::pickCell(const Grid& grid, float sx, float sy,
                        int& outX, int& outY, int& outZ)
{
    if (width <= 0 || height <= 0) return false;

    float ndcX = (2.0f * sx / (float)width) - 1.0f;
    float ndcY = 1.0f - (2.0f * sy / (float)height);

    glm::mat4 invVP = glm::inverse(proj * view);
    glm::vec4 near4 = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 far4  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    near4 /= near4.w;
    far4  /= far4.w;

    glm::vec3 origin = glm::vec3(near4);
    glm::vec3 dir    = glm::normalize(glm::vec3(far4 - near4));

    float bestT = 1e9f;
    bool hasHit = false;
    int hx = 0, hy = 0, hz = 0;
    glm::vec3 hitPos(0.0f);

    for (const auto& p : grid.cells) {
        if (!p.second.occupied) continue;

        int k = p.first;
        int bx =  k        & 0x3FF;
        int by = (k >> 10) & 0x3FF;
        int bz = (k >> 20) & 0x3FF;
        if (bx > 511) bx -= 1024;
        if (by > 511) by -= 1024;
        if (bz > 511) bz -= 1024;

        glm::vec3 bmin(bx - 0.5f, by - 0.5f, bz - 0.5f);
        glm::vec3 bmax(bx + 0.5f, by + 0.5f, bz + 0.5f);

        float tmin = 0.0f, tmax = 1e9f;
        bool hit = true;

        for (int i = 0; i < 3; i++) {
            float o = (&origin.x)[i];
            float d = (&dir.x)[i];
            float minB = (&bmin.x)[i];
            float maxB = (&bmax.x)[i];

            if (fabsf(d) < 1e-8f) {
                if (o < minB || o > maxB) { hit = false; break; }
                continue;
            }

            float t1 = (minB - o) / d;
            float t2 = (maxB - o) / d;
            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) { hit = false; break; }
        }

        if (hit && tmin > 0.001f && tmin < bestT) {
            bestT = tmin;
            hx = bx; hy = by; hz = bz;
            hitPos = origin + dir * tmin;
            hasHit = true;
        }
    }

    if (hasHit) {
        // Определяем грань
        glm::vec3 center((float)hx, (float)hy, (float)hz);
        glm::vec3 toHit = hitPos - center;

        float ax = fabsf(toHit.x);
        float ay = fabsf(toHit.y);
        float az = fabsf(toHit.z);

        int nx = 0, ny = 0, nz = 0;

        // Приоритет горизонтальным граням (чтобы легче делать ветки)
        if (ax > 0.35f && ax >= ay && ax >= az) {
            nx = (toHit.x > 0) ? 1 : -1;
        } else if (az > 0.35f && az >= ay) {
            nz = (toHit.z > 0) ? 1 : -1;
        } else {
            ny = (toHit.y > 0) ? 1 : -1;
        }

        outX = hx + nx;
        outY = hy + ny;
        outZ = hz + nz;

        // Если место занято — пробуем сверху
        if (grid.isOccupied(outX, outY, outZ)) {
            outX = hx;
            outY = hy + 1;
            outZ = hz;
        }
        return true;
    }

    // Земля
    if (fabsf(dir.y) < 1e-6f) return false;
    float t = -origin.y / dir.y;
    if (t < 0.0f) return false;

    glm::vec3 p = origin + dir * t;
    outX = (int)roundf(p.x);
    outY = 0;
    outZ = (int)roundf(p.z);
    return true;
}

bool Renderer::pickBlockToDelete(const Grid& grid, float sx, float sy,
                                 int& outX, int& outY, int& outZ)
{
    if (width <= 0 || height <= 0) return false;

    float ndcX = (2.0f * sx / (float)width) - 1.0f;
    float ndcY = 1.0f - (2.0f * sy / (float)height);

    glm::mat4 invVP = glm::inverse(proj * view);
    glm::vec4 near4 = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 far4  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    near4 /= near4.w;
    far4  /= far4.w;

    glm::vec3 origin = glm::vec3(near4);
    glm::vec3 dir    = glm::normalize(glm::vec3(far4 - near4));

    float bestT = 1e9f;
    bool found = false;

    for (const auto& p : grid.cells) {
        if (!p.second.occupied) continue;

        int k = p.first;
        int bx =  k        & 0x3FF;
        int by = (k >> 10) & 0x3FF;
        int bz = (k >> 20) & 0x3FF;
        if (bx > 511) bx -= 1024;
        if (by > 511) by -= 1024;
        if (bz > 511) bz -= 1024;

        // Немного увеличиваем AABB, чтобы легче попадать
        glm::vec3 bmin(bx - 0.55f, by - 0.55f, bz - 0.55f);
        glm::vec3 bmax(bx + 0.55f, by + 0.55f, bz + 0.55f);

        float tmin = 0.0f;
        float tmax = 1e9f;
        bool hit = true;

        for (int i = 0; i < 3; i++) {
            float o = (&origin.x)[i];
            float d = (&dir.x)[i];
            float minB = (&bmin.x)[i];
            float maxB = (&bmax.x)[i];

            if (fabsf(d) < 1e-8f) {
                if (o < minB || o > maxB) {
                    hit = false;
                    break;
                }
                continue;
            }

            float t1 = (minB - o) / d;
            float t2 = (maxB - o) / d;
            if (t1 > t2) std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            if (tmin > tmax) {
                hit = false;
                break;
            }
        }

        if (hit && tmin > 0.0005f && tmin < bestT) {
            bestT = tmin;
            outX = bx;
            outY = by;
            outZ = bz;
            found = true;
        }
    }

    return found;
}

void Renderer::pan(float dx, float dy) {
    // Получаем правый и верхний векторы камеры
    glm::vec3 forward = glm::normalize(camTarget - glm::vec3(
            camTarget.x + camDistance * std::cos(camPitch) * std::sin(camYaw),
            camTarget.y + camDistance * std::sin(camPitch),
            camTarget.z + camDistance * std::cos(camPitch) * std::cos(camYaw)
    ));

    // Более простой и стабильный способ:
    glm::vec3 right = glm::normalize(glm::vec3(
            std::cos(camYaw), 0.0f, -std::sin(camYaw)
    ));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float speed = camDistance * 0.0018f;  // чувствительность

    camTarget += right * (-dx * speed);
    camTarget += up * (dy * speed);

    updateCamera();
}