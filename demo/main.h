#pragma once

#include <coroutine>

struct GLFWwindow;

struct awaitable {
    bool await_ready();
    void await_suspend(std::coroutine_handle<> h);
    void await_resume();
    GLFWwindow *window;
};

awaitable animation_frame(struct GLFWwindow *window);
 
struct task {
    struct promise_type {
        task get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() {}
    };
};

extern const int glfw_api;
