cmake_minimum_required(VERSION 3.22.1)
project("towner")

add_library(towner SHARED
        native-lib.cpp
        Renderer.cpp
        Grid.cpp
        Shader.cpp
)

target_include_directories(towner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/glm
)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(gles-lib GLESv3)

target_link_libraries(towner
        ${log-lib}
        ${android-lib}
        ${egl-lib}
        ${gles-lib}
)