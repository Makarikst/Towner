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
precision mediump float;
in vec3 vNormal;
in vec3 vWorldPos;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.45, 1.0, 0.35));

    float diff = max(dot(normal, lightDir), 0.0);
    float ambient = 0.42;
    float lighting = ambient + diff * 0.65;

    // Лёгкий rim-light, чтобы кубы выглядели объёмнее
    vec3 viewDir = normalize(vec3(0.0, 0.3, 1.0));
    float rim = 1.0 - max(dot(normal, viewDir), 0.0);
    rim = pow(rim, 2.5) * 0.18;

    vec3 finalColor = uColor * lighting + vec3(rim);
    FragColor = vec4(finalColor, 1.0);
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

    // Настоящий куб с правильными нормалями
    float cube[] = {
            // Back face
            -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,   0.0f,  0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,   0.0f,  0.0f, -1.0f,

            // Front face
            -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,   0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,   0.0f,  0.0f,  1.0f,

            // Left face
            -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  -1.0f,  0.0f,  0.0f,

            // Right face
            0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
            0.5f,  0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
            0.5f, -0.5f, -0.5f,   1.0f,  0.0f,  0.0f,
            0.5f, -0.5f,  0.5f,   1.0f,  0.0f,  0.0f,
            0.5f,  0.5f,  0.5f,   1.0f,  0.0f,  0.0f,

            // Bottom face
            -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
            0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
            0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,   0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,   0.0f, -1.0f,  0.0f,

            // Top face
            -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
            0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
            0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,   0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,   0.0f,  1.0f,  0.0f,
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

    glClearColor(0.53f, 0.78f, 0.96f, 1.0f);

    updateCamera();
    LOGI("Renderer init done");
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
    if (w <= 0 || h <= 0) return;
    width = w;
    height = h;
    glViewport(0, 0, w, h);

    float aspect = static_cast<float>(w) / static_cast<float>(h);
    proj = glm::perspective(glm::radians(42.0f), aspect, 0.1f, 200.0f);
    updateCamera();
}

void Renderer::draw(const Grid& grid) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
                        int& outX, int& outY, int& outZ) {
    if (width <= 0 || height <= 0) return false;

    float ndcX = (2.0f * sx / width) - 1.0f;
    float ndcY = 1.0f - (2.0f * sy / height);

    glm::mat4 invVP = glm::inverse(proj * view);
    glm::vec4 nearP = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farP  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    nearP /= nearP.w;
    farP  /= farP.w;

    glm::vec3 origin = glm::vec3(nearP);
    glm::vec3 dir = glm::normalize(glm::vec3(farP - nearP));

    // Ray vs AABB
    float bestT = 1e9f;
    bool hitSomething = false;
    int hitX = 0, hitY = 0, hitZ = 0;

    for (const auto& pair : grid.cells) {
        if (!pair.second.occupied) continue;

        int k = pair.first;
        int bx =  k        & 0x3FF;
        int by = (k >> 10) & 0x3FF;
        int bz = (k >> 20) & 0x3FF;
        if (bx > 511) bx -= 1024;
        if (by > 511) by -= 1024;
        if (bz > 511) bz -= 1024;

        glm::vec3 minP(bx - 0.5f, by - 0.5f, bz - 0.5f);
        glm::vec3 maxP(bx + 0.5f, by + 0.5f, bz + 0.5f);

        float tmin = 0.0f, tmax = 1e9f;
        bool hit = true;
        for (int i = 0; i < 3; ++i) {
            float o = (&origin.x)[i];
            float d = (&dir.x)[i];
            float invD = 1.0f / (d + 1e-8f);
            float t1 = ((&minP.x)[i] - o) * invD;
            float t2 = ((&maxP.x)[i] - o) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) { hit = false; break; }
        }

        if (hit && tmin < bestT && tmin > 0.0f) {
            bestT = tmin;
            hitX = bx; hitY = by; hitZ = bz;
            hitSomething = true;
        }
    }

    if (hitSomething) {
        outX = hitX;
        outY = hitY + 1;   // ставим сверху
        outZ = hitZ;
        return true;
    }

    // Земля
    if (std::fabs(dir.y) < 1e-5f) return false;
    float t = (0.0f - origin.y) / dir.y;
    if (t < 0.0f) return false;

    glm::vec3 hit = origin + dir * t;
    outX = (int)std::floor(hit.x + 0.5f);
    outY = 0;
    outZ = (int)std::floor(hit.z + 0.5f);
    return true;
}