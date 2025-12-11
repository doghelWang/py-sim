cmake_minimum_required(VERSION 3.14)
project(PyEmbedGUI LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Dependencies
# -----------------------------------------------------------------------------

# 1. Pybind11
include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.11.1
)
FetchContent_MakeAvailable(pybind11)

# 2. GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.3.8
)
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE INTERNAL "Generate installation target")
FetchContent_MakeAvailable(glfw)

# 3. Dear ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        v1.89.9
)
FetchContent_MakeAvailable(imgui)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(IMGUI_DIR ${imgui_SOURCE_DIR})
set(SOURCES
    src/main.cpp
    src/HostApi.cpp
    src/PythonEngine.cpp
    src/GuiLayer.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(demo_gui ${SOURCES})

# Include directories for ImGui and our Headers
target_include_directories(demo_gui PRIVATE 
    include
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
)

# Link libraries
if(MSVC)
    target_link_libraries(demo_gui PRIVATE 
        pybind11::embed
        glfw
        opengl32
    )
    # Set startup project to not pop console in Release (optional)
    set_property(TARGET demo_gui PROPERTY WIN32_EXECUTABLE ON)
elseif(APPLE)
    target_link_libraries(demo_gui PRIVATE 
        pybind11::embed
        glfw
        "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo"
    )
else() # Linux / Unix
    target_link_libraries(demo_gui PRIVATE 
        pybind11::embed
        glfw
        GL
        dl
        pthread
    )
endif()
