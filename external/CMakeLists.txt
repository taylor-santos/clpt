include(FetchContent)

add_subdirectory(glad)

# Don't re-download external dependencies if they are already present
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

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
        GIT_TAG 45ce5ddd197d5c58f50fdd3296a5131c894e5527 # 3.3.7
)
FetchContent_MakeAvailable(glfw)

#------------------------------------------------------------------------------------
# Fetch glm
#------------------------------------------------------------------------------------

message(STATUS "Fetching glm...")
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm
        GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e # 0.9.9.8
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
        GIT_TAG 4d8716f1efc1d14aa736ef52ee727bd4204f4c40 # 2.4.6
)
FetchContent_MakeAvailable(doctest)

#------------------------------------------------------------------------------------
# Fetch imgui
#------------------------------------------------------------------------------------

message(STATUS "Fetching imgui...")
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG dc8c3618e8f8e2dada23daa1aa237626af341fd8 # docking/1.85
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
            PUBLIC glad glfw)
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
