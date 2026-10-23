#include <memory>

#include <glm/glm.hpp>

namespace imv {

    struct editor {
        editor();
        ~editor();
        std::unique_ptr<struct editor_data> d;
    };
    
    extern struct editor* global_editor;

    struct inputs {
        struct {
            float x, y;
            bool primary;
        } mouse;
    };

    void set_inputs(const inputs& i);

    glm::vec2 edit(glm::vec2& v);

}
