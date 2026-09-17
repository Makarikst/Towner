#include "Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <android/log.h>

#define LOG_TAG "Towner"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static const char* VS = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
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
    float diff = max(dot(normalize(vNormal), lightDir), 0.3);
    FragColor = vec4(uColor * diff, 1.0);
}
)";

void Renderer::init() {
    if (!shader.compile(VS, FS)) {
        LOGE("Shader compile FAILED");
        return;
    }
    LOGI("Shader compiled OK, id=%u", shader.id);

    uMVP = glGetUniformLocation(shader.id, "uMVP");
    uModel = glGetUniformLocation(shader.id, "uModel");
    uColor = glGetUniformLocation(shader.id, "uColor");

    float cube[] = {
            -0.5f,-0.5f,-0.5f, 0,0,-1,   0.5f,-0.5f,-0.5f, 0,0,-1,   0.5f, 0.5f,-0.5f, 0,0,-1,
            0.5f, 0.5f,-0.5f, 0,0,-1,  -0.5f, 0.5f,-0.5f, 0,0,-1,  -0.5f,-0.5f,-0.5f, 0,0,-1,
            -0.5f,-0.5f, 0.5f, 0,0,1,   0.5f,-0.5f, 0.5f, 0,0,1,   0.5f, 0.5f, 0.5f, 0,0,1,
            0.5f, 0.5f, 0.5f, 0,0,1,  -0.5f, 0.5f, 0.5f, 0,0,1,  -0.5f,-0.5f, 0.5f, 0,0,1,
            -0.5f, 0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f,-0.5f,-1,0,0,  -0.5f,-0.5f,-0.5f,-1,0,0,
            -0.5f,-0.5f,-0.5f,-1,0,0,  -0.5f,-0.5f, 0.5f,-1,0,0,  -0.5f, 0.5f, 0.5f,-1,0,0,
            0.5f, 0.5f, 0.5f, 1,0,0,   0.5f, 0.5f,-0.5f, 1,0,0,   0.5f,-0.5f,-0.5f, 1,0,0,
            0.5f,-0.5f,-0.5f, 1,0,0,   0.5f,-0.5f, 0.5f, 1,0,0,   0.5f, 0.5f, 0.5f, 1,0,0,
            -0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f,-0.5f, 0,-1,0,  0.5f,-0.5f, 0.5f, 0,-1,0,
            0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f, 0.5f, 0,-1,0, -0.5f,-0.5f,-0.5f, 0,-1,0,
            -0.5f, 0.5f,-0.5f, 0,1,0,   0.5f, 0.5f,-0.5f, 0,1,0,   0.5f, 0.5f, 0.5f, 0,1,0,
            0.5f, 0.5f, 0.5f, 0,1,0,  -0.5f, 0.5f, 0.5f, 0,1,0,  -0.5f, 0.5f,-0.5f, 0,1,0,
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);  // КРАСНЫЙ для теста
    LOGI("Renderer init done, cubeVAO=%u", cubeVAO);
}

void Renderer::resize(int w, int h) {
    width = w; height = h;
    glViewport(0, 0, w, h);
    float aspect = (float)w / (float)h;
    proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);

    view = glm::lookAt(
            glm::vec3(8.0f, 8.0f, 8.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
    );
}

void Renderer::draw(const Grid& grid) {
    static int frame = 0;
    frame++;
    if (frame % 120 == 0) {
        LOGI("DRAW: blocks=%d shader=%u vao=%u", grid.count(), shader.id, cubeVAO);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //shader.use();
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

    //glm::mat4 vp = proj * view;
    //glUniformMatrix4fv(uMVP, 1, GL_FALSE, glm::value_ptr(vp));

    //glBindVertexArray(cubeVAO);
    /*for (auto& pair : grid.cells) {
        if (!pair.second.occupied) continue;

        int k = pair.first;
        int x = k & 0x3FF;
        int y = (k >> 10) & 0x3FF;
        int z = (k >> 20) & 0x3FF;
        if (x > 511) x -= 1024;
        if (y > 511) y -= 1024;
        if (z > 511) z -= 1024;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        glUniformMatrix4fv(uModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(uColor, 1, glm::value_ptr(pair.second.color));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);*/
}

void Renderer::destroy() {
    shader.destroy();
    if (cubeVAO) glDeleteVertexArrays(1, &cubeVAO);
    if (cubeVBO) glDeleteBuffers(1, &cubeVBO);
}

bool Renderer::pickCell(const Grid& grid, float sx, float sy,
                        int& outX, int& outY, int& outZ) {
    float ndcX = (2.0f * sx / width) - 1.0f;
    float ndcY = 1.0f - (2.0f * sy / height);

    glm::mat4 invVP = glm::inverse(proj * view);
    glm::vec4 nearP = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farP  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    nearP /= nearP.w;
    farP  /= farP.w;

    glm::vec3 origin = glm::vec3(nearP);
    glm::vec3 dir = glm::normalize(glm::vec3(farP - nearP));

    if (fabs(dir.y) < 1e-5f) return false;
    float t = (0.5f - origin.y) / dir.y;
    if (t < 0) return false;

    glm::vec3 hit = origin + dir * t;
    outX = (int)floor(hit.x + 0.5f);
    outY = 0;
    outZ = (int)floor(hit.z + 0.5f);
    return true;
}