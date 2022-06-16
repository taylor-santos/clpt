find_package(OpenCL REQUIRED)
find_package(OpenGL REQUIRED)

set(LIB_NAME
        ${PROJECT_NAME}_lib)

set(BUILD_SRC
        src/clpt.cpp
        src/glfwcpp.cpp
        src/render.cpp
        src/transform.cpp
        src/camera.cpp)

set(PUBLIC_LIBS
        gl3w
        glfw
        glm
        imgui
        OpenCL::HeadersCpp
        OpenCL::OpenCL
        ${OPENGL_gl_LIBRARY})

add_library(${LIB_NAME}
        ${BUILD_SRC})

target_link_libraries(${LIB_NAME}
        PUBLIC ${PUBLIC_LIBS})

target_include_directories(${LIB_NAME}
        PUBLIC include)

if (MSVC)
    target_compile_options(${LIB_NAME}
            PRIVATE /W4 /WX)
    target_compile_definitions(${LIB_NAME}
            PRIVATE _CRT_SECURE_NO_WARNINGS)
else ()
    target_compile_options(${LIB_NAME}
            PRIVATE -Wall -Wextra -pedantic -Werror)
endif ()
