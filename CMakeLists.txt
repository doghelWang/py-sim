cmake_minimum_required(VERSION 3.14)
project(PySim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- Dependencies ---

# 1. Google Test
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent GTest from overriding /MD with /MT
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# 2. pybind11 (Embedded Python)
FetchContent_Declare(
  pybind11
  URL https://github.com/pybind/pybind11/archive/refs/tags/v2.11.1.zip
)
FetchContent_MakeAvailable(pybind11)

# 3. cpp-httplib (Web Server)
FetchContent_Declare(
  cpp-httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.14.3.zip
)
FetchContent_MakeAvailable(cpp-httplib)

# 4. nlohmann_json (JSON)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)


# --- Core Logic ---
file(GLOB_RECURSE SOURCES 
    "src/amr/*.cpp"
    "src/HostApi.cpp"
)
# Exclude main.cpp from library to allow testing
list(FILTER SOURCES EXCLUDE REGEX "main.cpp$")

add_library(amr_core ${SOURCES})

target_include_directories(amr_core PUBLIC 
    include 
    ${python_INCLUDE_DIRS}
)

target_link_libraries(amr_core PUBLIC 
    pybind11::embed
    httplib::httplib
    nlohmann_json::nlohmann_json
)

# --- Executable (Server) ---
add_executable(py_sim_server src/main.cpp)
target_link_libraries(py_sim_server PRIVATE amr_core)

# --- Tests ---
enable_testing()
add_subdirectory(tests)


# Copy Web Root to build dir
add_custom_command(TARGET py_sim_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/web_root
    $<TARGET_FILE_DIR:py_sim_server>/web_root
)

# Copy Scripts to build dir
add_custom_command(TARGET py_sim_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/scripts
    $<TARGET_FILE_DIR:py_sim_server>/scripts
)



if(WIN32)
    target_link_libraries(py_sim_server PRIVATE ws2_32)
    target_compile_definitions(py_sim_server PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(py_sim_server PRIVATE /utf-8)
endif()
