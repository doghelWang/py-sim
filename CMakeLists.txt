cmake_minimum_required(VERSION 3.14)
project(demo_gui)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# 1. GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.8
)
FetchContent_MakeAvailable(glfw)

# 2. ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.90
)
FetchContent_MakeAvailable(imgui)

# 3. pybind11
find_package(Python 3 COMPONENTS Interpreter Development REQUIRED)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# 4. cpp-httplib
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.15.3
)
FetchContent_MakeAvailable(httplib)

# 5. stb
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# 6. GoogleTest (For Unit Testing)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)


enable_testing()
add_subdirectory(tests)

# --- Configuration ---


set(IMGUI_DIR ${imgui_SOURCE_DIR})

# Core Library (No Main)
add_library(amr_core STATIC
    src/amr/AppModel.cpp
    src/amr/Hardware.cpp
    src/amr/AmrController.cpp
    src/amr/SimulatorCore.cpp
    # src/GuiLayer.cpp # Dependencies on legacy AppState prevent this yet
    # src/HostApi.cpp 
)
target_include_directories(amr_core PUBLIC include)

add_executable(demo_gui 
    src/main.cpp 
    src/GuiLayer.cpp
    src/gui/SimulatorView.cpp
    src/gui/HardwareView.cpp
    src/gui/EditorView.cpp
    src/HostApi.cpp 
    src/PythonEngine.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_link_libraries(demo_gui PRIVATE amr_core)

target_include_directories(demo_gui PRIVATE 
    include
    ${httplib_SOURCE_DIR}
    ${stb_SOURCE_DIR}
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
)

target_link_libraries(demo_gui PRIVATE 
    glfw 
    pybind11::embed
)

if(APPLE)
    target_link_libraries(demo_gui PRIVATE "-framework OpenGL" "-framework IOKit" "-framework Cocoa")
elseif(UNIX)
    target_link_libraries(demo_gui PRIVATE GL)
else()
    target_link_libraries(demo_gui PRIVATE opengl32 ws2_32)
    target_compile_definitions(demo_gui PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(demo_gui PRIVATE /utf-8)
endif()
