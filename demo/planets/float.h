#include <array>

inline float sum(float a, float b, float &error) {
    float s = a + b;
    float z = s - a;
    error = b - z;
    return s;
}

struct float96_t {
    float offsets[3];

    float96_t() : float96_t(0) {}
    float96_t(float value) : offsets(value) {}
    operator float() {
        return offsets[0];
    }
    
    float96_t operator+(float summand) {
        float96_t result;
        float error = summand;
        for (int i = 0; i < std::size(offsets); i++) {
            result.offsets[i] = sum(offsets[i], error, error);
        }
        return result;
    }
    float96_t operator+(float96_t summand) {
        float96_t result;
        for (int i = 0; i < std::size(offsets); i++) {
            result = result + offsets[i];
            result = result + summand.offsets[i];
        }
        return result;
    }
    float96_t operator-(float96_t summand) {
        return *this + -summand;
    }
    float96_t operator-() {
        float96_t result;
        for (int i = 0; i < std::size(offsets); i++)
            result.offsets[i] = -offsets[i];
        return result;
    }
};
