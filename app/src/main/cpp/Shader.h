#pragma once
#include <GLES3/gl3.h>

class Shader {
public:
    GLuint id = 0;
    bool compile(const char* vertexSrc, const char* fragmentSrc);
    void use() const;
    void destroy();
};