#include "main.h"

#include <coroutine>

#include <GLFW/glfw3.h>

bool awaitable::await_ready() { return true; }
void awaitable::await_suspend(std::coroutine_handle<> h) {
    h.resume();
}
void awaitable::await_resume() {}

awaitable animation_frame(GLFWwindow *window) {
    return awaitable{window};
}

task game_main();

const int glfw_api = GLFW_NO_API;

int main() {
    game_main();
}
