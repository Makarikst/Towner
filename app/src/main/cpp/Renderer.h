#pragma once
#include <GLES3/gl3.h>
#include <glm/glm.hpp>
#include "Grid.h"
#include "Shader.h"

class Renderer {
public:
    void init();
    void resize(int w, int h);
    void draw(const Grid& grid);
    void destroy();

    bool pickCell(const Grid& grid, float screenX, float screenY,
                  int& outX, int& outY, int& outZ);

    // Управление камерой
    void orbit(float dx, float dy);
    void zoom(float factor);

    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    int width = 0, height = 0;

private:
    void updateCamera();

    Shader shader;
    GLuint cubeVAO = 0, cubeVBO = 0;
    GLint uMVP = -1, uModel = -1, uColor = -1;

    // Параметры orbit-камеры
    float camDistance = 18.0f;
    float camYaw = 0.7f;      // радианы
    float camPitch = 0.55f;   // радианы
    glm::vec3 camTarget = glm::vec3(0.0f, 1.5f, 0.0f);
};