#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#include "GuiLayer.hpp"
#include "PythonEngine.hpp"
#include "amr/AppModel.hpp" // Use AppModel
#include <pybind11/embed.h>
// Note: HostApi uses PYBIND11_EMBEDDED_MODULE which registers itself globally.

namespace py = pybind11;

static void glfw_error_callback(int error, const char *description) {
  fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char **) {
  // 1. Setup Python Interpreter
  py::scoped_interpreter guard{};
  py::gil_scoped_release release;

  // 2. Setup Window
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit())
    return 1;

#if defined(IMGUI_IMPL_OPENGL_ES2)
  const char *glsl_version = "#version 100";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
  const char *glsl_version = "#version 150";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
  const char *glsl_version = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

  GLFWwindow *window =
      glfwCreateWindow(1280, 800, "EmbedCtrl (Refactored)", NULL, NULL);
  if (window == NULL)
    return 1;
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  // 3. Setup ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  GuiLayer::SetupStyle();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // 3b. Setup Safety Inputs
  // Safety Inputs are now configured by the Python script (visual_prog.py) via
  // configure_input()
  auto &app = amr::AppModel::Instance();

  // 4. Main Loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Safety Update (Independent of GUI/Python)
    app.UpdateSafetyLogic(0.016f); // Approx 60Hz dt

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    GuiLayer::Render(window);

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  amr::AppModel::Instance()
      .RequestTermination(); // Sets terminate flag and unpauses

  // Ensure we wait for python to finish
  PythonEngine::JoinWorker();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
