#include "window.h"
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

WindowAttempt open_vulkan_window(const WindowConfig& config) {
    WindowAttempt out{};

    int init = glfwInit();
    if (init != GLFW_TRUE) {
        const char* desc = nullptr;
        glfwGetError(&desc);
        out.error = (desc && *desc) ? std::string(desc)
                                    : std::string("glfwInit returned GLFW_FALSE");
        out.stage = WindowAttempt::Stage::GlfwInit;
        return out;   // GLFW 失败时内部已撤回，不调 terminate
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window;
    window = glfwCreateWindow(config.w, config.h, config.title,
                             nullptr, nullptr);
    if(!window){
        const char* desc = nullptr;
        glfwGetError(&desc);
        out.error = (desc && *desc) ? std::string(desc) : std::string("window creation failed");
        out.stage = WindowAttempt::Stage::CreateWindow;
        glfwTerminate();

        return out;
    }

    out.session.window = window;
    out.session.glfw_initialized = true;
    out.stage = WindowAttempt::Stage::Created;
    out.error.clear();
    return out;
}
WindowExtensionsAttempt required_window_instance_extensions(const WindowSession& session) {
    (void)session;
    // Query once, preserve GLFW-owned names, and return a clean failure result.
    WindowExtensionsAttempt out{};

    std::uint32_t count = 0;
    const char** names = glfwGetRequiredInstanceExtensions(&count);

    if (names == nullptr||count ==0) {
        // 失败：GLFW 可能没设错误码，desc 可能为 null，error 必须自己兜底。
        const char* desc = nullptr;
        glfwGetError(&desc);   // 返回 0 表示“没有错误”，不影响失败判定
        out.ok = false;
        out.count = 0;
        out.names = nullptr;
        out.error = (desc && *desc)
                        ? std::string(desc)
                        : std::string("glfwGetRequiredInstanceExtensions returned NULL");
        return out;
    }
    out.ok = true;
    out.count = count;
    out.names = names;   // const char** -> const char* const* 隐式转换合法
    out.error.clear();
    return out;
}
void close_vulkan_window(WindowSession& session) noexcept {
    if(session.window){
        glfwDestroyWindow(session.window);
    }
    if(session.glfw_initialized)glfwTerminate();
    session.window = nullptr;
    session.glfw_initialized = false;
}
