#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <android/log.h>
#include <cmath>

#define LOG_TAG "Towner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* VS = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNormal;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vNormal = mat3(uModel) * aNormal;
}
)";

static const char* FS = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(vNormal), lightDir), 0.25);
    FragColor = vec4(uColor * diff, 1.0);
}
)";

void Renderer::init() {
    if (!shader.compile(VS, FS)) {
        LOGE("Shader compile FAILED");
        return;
    }
    LOGI("Shader compiled OK, id=%u", shader.id);

    uMVP   = glGetUniformLocation(shader.id, "uMVP");
    uModel = glGetUniformLocation(shader.id, "uModel");
    uColor = glGetUniformLocation(shader.id, "uColor");

    // Куб с нормалями (36 вершин)
    float cube[] = {
            // back
            -0.5f,-0.5f,-0.5f,  0, 0,-1,   0.5f,-0.5f,-0.5f,  0, 0,-1,   0.5f, 0.5f,-0.5f,  0, 0,-1,
            0.5f, 0.5f,-0.5f,  0, 0,-1,  -0.5f, 0.5f,-0.5f,  0, 0,-1,  -0.5f,-0.5f,-0.5f,  0, 0,-1,
            // front
            -0.5f,-0.5f, 0.5f,  0, 0, 1,   0.5f,-0.5f, 0.5f,  0, 0, 1,   0.5f, 0.5f, 0.5f,  0, 0, 1,
            0.5f, 0.5f, 0.5f,  0, 0, 1,  -0.5f, 0.5f, 0.5f,  0, 0, 1,  -0.5f,-0.5f, 0.5f,  0, 0, 1,
            // left
            -0.5f, 0.5f, 0.5f, -1, 0, 0,  -0.5f, 0.5f,-0.5f, -1, 0, 0,  -0.5f,-0.5f,-0.5f, -1, 0, 0,
            -0.5f,-0.5f,-0.5f, -1, 0, 0,  -0.5f,-0.5f, 0.5f, -1, 0, 0,  -0.5f, 0.5f, 0.5f, -1, 0, 0,
            // right
            0.5f, 0.5f, 0.5f,  1, 0, 0,   0.5f, 0.5f,-0.5f,  1, 0, 0,   0.5f,-0.5f,-0.5f,  1, 0, 0,
            0.5f,-0.5f,-0.5f,  1, 0, 0,   0.5f,-0.5f, 0.5f,  1, 0, 0,   0.5f, 0.5f, 0.5f,  1, 0, 0,
            // bottom
            -0.5f,-0.5f,-0.5f,  0,-1, 0,   0.5f,-0.5f,-0.5f,  0,-1, 0,   0.5f,-0.5f, 0.5f,  0,-1, 0,
            0.5f,-0.5f, 0.5f,  0,-1, 0,  -0.5f,-0.5f, 0.5f,  0,-1, 0,  -0.5f,-0.5f,-0.5f,  0,-1, 0,
            // top
            -0.5f, 0.5f,-0.5f,  0, 1, 0,   0.5f, 0.5f,-0.5f,  0, 1, 0,   0.5f, 0.5f, 0.5f,  0, 1, 0,
            0.5f, 0.5f, 0.5f,  0, 1, 0,  -0.5f, 0.5f, 0.5f,  0, 1, 0,  -0.5f, 0.5f,-0.5f,  0, 1, 0,
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

    // Небо / фон (приятный голубоватый)
    glClearColor(0.55f, 0.75f, 0.95f, 1.0f);

    LOGI("Renderer init done, cubeVAO=%u", cubeVAO);
}

void Renderer::resize(int w, int h) {
    if (w <= 0 || h <= 0) return;

    width = w;
    height = h;
    glViewport(0, 0, w, h);

    float aspect = static_cast<float>(w) / static_cast<float>(h);
    proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);

    // Камера как в Townscaper — немного сверху и сбоку
    view = glm::lookAt(
            glm::vec3(10.0f, 12.0f, 10.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
    );
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

        // Восстанавливаем отрицательные координаты (10-битный signed)
        if (x > 511) x -= 1024;
        if (y > 511) y -= 1024;
        if (z > 511) z -= 1024;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x),
                                                                    static_cast<float>(y),
                                                                    static_cast<float>(z)));

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
    if (cubeVAO) {
        glDeleteVertexArrays(1, &cubeVAO);
        cubeVAO = 0;
    }
    if (cubeVBO) {
        glDeleteBuffers(1, &cubeVBO);
        cubeVBO = 0;
    }
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
    glm::vec3 dir    = glm::normalize(glm::vec3(farP - nearP));

    // Пересекаем с плоскостью y = 0 (земля)
    if (std::fabs(dir.y) < 1e-5f) return false;

    float t = (0.0f - origin.y) / dir.y;
    if (t < 0.0f) return false;

    glm::vec3 hit = origin + dir * t;

    outX = static_cast<int>(std::floor(hit.x + 0.5f));
    outY = 0;
    outZ = static_cast<int>(std::floor(hit.z + 0.5f));

    return true;
}