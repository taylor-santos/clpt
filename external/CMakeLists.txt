include(FetchContent)

# Don't re-download external dependencies if they are already present
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(Python COMPONENTS Interpreter REQUIRED)

#------------------------------------------------------------------------------------
# Fetch OpenCL Headers
#------------------------------------------------------------------------------------

message(STATUS "Fetching OpenCL Headers...")
set(OPENCL_HEADERS_BUILD_CXX_TESTS OFF)
FetchContent_Declare(
        clh
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-Headers.git
        GIT_TAG v2022.05.18
)
FetchContent_MakeAvailable(clh)

#------------------------------------------------------------------------------------
# Fetch OpenCL C++ Bindings
#------------------------------------------------------------------------------------

message(STATUS "Fetching OpenCL C++ Bindings...")
# set(OPENCL_CLHPP_HEADERS_DIR ${OpenCL_INCLUDE_DIRS})
set(BUILD_DOCS OFF)
set(BUILD_EXAMPLES OFF)
FetchContent_Declare(
        clhpp
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-CLHPP
        GIT_TAG v2022.05.18
)
FetchContent_MakeAvailable(clhpp)

#------------------------------------------------------------------------------------
# Fetch OpenCL ICD LOADER
#------------------------------------------------------------------------------------

message(STATUS "Fetching OpenCL ICD Loader...")
# set(OPENCL_CLHPP_HEADERS_DIR ${OpenCL_INCLUDE_DIRS})
FetchContent_Declare(
        clicd
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenCL-ICD-Loader.git
        GIT_TAG v2022.05.18
)
FetchContent_MakeAvailable(clicd)

#------------------------------------------------------------------------------------
# Fetch gl3w
#------------------------------------------------------------------------------------

message(STATUS "Fetching gl3w...")
FetchContent_Declare(
        gl3w
        GIT_REPOSITORY https://github.com/skaslev/gl3w
        GIT_TAG master
)
FetchContent_GetProperties(gl3w)
if (NOT gl3w_POPULATED)
    FetchContent_Populate(gl3w)

    find_file(gl3w_GEN_PY "gl3w_gen.py" PATHS "${gl3w_SOURCE_DIR}" NO_DEFAULT_PATH)
    if (NOT gl3w_GEN_PY)
        message(FATAL_ERROR "could not find gl3w_gen.py")
    endif ()
    execute_process(
            COMMAND ${PYTHON_EXECUTABLE} ${gl3w_GEN_PY}
            WORKING_DIRECTORY ${gl3w_SOURCE_DIR}
            RESULT_VARIABLE gl3w_GEN_PY_RESULT)
    if (NOT gl3w_GEN_PY_RESULT EQUAL "0")
        message(FATAL_ERROR "bad exit status from call to gl3w_gen.py")
    endif ()
    add_library(gl3w)
    target_sources(gl3w PRIVATE
            "${gl3w_SOURCE_DIR}/src/gl3w.c")

    target_include_directories(gl3w PUBLIC "${gl3w_SOURCE_DIR}/include")
endif ()

#------------------------------------------------------------------------------------
# Fetch glfw
#------------------------------------------------------------------------------------

message(STATUS "Fetching glfw...")
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw
        GIT_TAG 3.3.7
)
FetchContent_MakeAvailable(glfw)

#------------------------------------------------------------------------------------
# Fetch glm
#------------------------------------------------------------------------------------

message(STATUS "Fetching glm...")
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

#------------------------------------------------------------------------------------
# Fetch doctest
#------------------------------------------------------------------------------------

message(STATUS "Fetching doctest...")
set(DOCTEST_WITH_TESTS OFF)
set(DOCTEST_NO_INSTALL ON)
set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF)
FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/onqtam/doctest
        GIT_TAG v2.4.8
)
FetchContent_MakeAvailable(doctest)

#------------------------------------------------------------------------------------
# Fetch imgui
#------------------------------------------------------------------------------------

message(STATUS "Fetching imgui...")
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG v1.87
)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
    FetchContent_Populate(imgui)

    add_library(imgui
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
    target_link_libraries(imgui
            PUBLIC gl3w glfw)
    target_include_directories(imgui
            PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
    set_target_properties(imgui PROPERTIES
            CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(imgui
                PRIVATE -fPIC -Wno-deprecated-enum-enum-conversion)
        if (MINGW)
            target_link_libraries(imgui
                    PRIVATE imm32)
        endif ()
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(imgui
                PRIVATE -fPIC)
    endif ()
endif ()
