// 窗口模块：用 GLFW 创建/关闭不带 OpenGL 上下文的窗口，并查询创建 Vulkan 实例所需的窗口扩展。
#pragma once
#include <cstdint>
#include <string>
struct GLFWwindow;

// Given declarations from the student's approved §65/§66 draft. Codex scaffold.
struct WindowConfig { int w; int h; const char* title; };
struct WindowSession {
    GLFWwindow* window = nullptr;
    bool glfw_initialized = false;
};
struct WindowAttempt {
    enum class Stage { NotStarted, GlfwInit, CreateWindow, Created };
    // GlfwInit/CreateWindow denote the failed stage, not a completed stage.
    Stage stage = Stage::NotStarted;
    WindowSession session{};
    std::string error;
    bool ok() const noexcept { return stage == Stage::Created; }
};
struct WindowExtensionsAttempt {
    bool ok = false;
    std::uint32_t count = 0;
    // Borrowed from GLFW; array and strings must not outlive glfwTerminate.
    const char* const* names = nullptr;
    std::string error;
};
// One main-thread session, no other GLFW owner. Positive dimensions/valid title.
// Success transfers one manual cleanup responsibility; failure leaves none.
WindowAttempt open_vulkan_window(const WindowConfig& config);
// Requires a successful live session. Borrows only; never cleans up the session.
WindowExtensionsAttempt required_window_instance_extensions(const WindowSession& session);
// Clears this session. No copying of cleanup responsibility; no other objects owned.
void close_vulkan_window(WindowSession& session) noexcept;
