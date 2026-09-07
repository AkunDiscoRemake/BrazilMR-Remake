// BrazilMR — tipos de hand tracking (compartilhado nativo ↔ Kotlin ↔ MediaPipe).
#pragma once

#include "../math/MathTypes.hpp"

namespace brazilmr {

// 21 landmarks no padrão MediaPipe Hands:
//  0 pulso | 1-4 polegar | 5-8 indicador | 9-12 médio
//  13-16 anelar | 17-20 mínimo
constexpr int kNumHandLandmarks = 21;

enum class HandGesture : int {
    NONE      = 0,
    OPEN_PALM = 1,   // mão aberta (abrir menu)
    PINCH     = 2,   // pinça (selecionar/clicar)
    GRAB      = 3,   // agarrar (mover janela/objeto)
    FIST      = 4,   // punho (comando configurável)
    POINT     = 5,   // apontar (raycast)
    SWIPE     = 6,   // navegação (transitório)
};

inline const char* gestureName(HandGesture g) {
    switch (g) {
        case HandGesture::OPEN_PALM: return "OPEN_PALM";
        case HandGesture::PINCH:     return "PINCH";
        case HandGesture::GRAB:      return "GRAB";
        case HandGesture::FIST:      return "FIST";
        case HandGesture::POINT:     return "POINT";
        case HandGesture::SWIPE:     return "SWIPE";
        default: return "NONE";
    }
}

// Estado de uma mão no espaço do MUNDO (já transformado pela pose da cabeça).
struct HandState {
    bool present = false;
    bool left = false;
    Vec3 landmarks[kNumHandLandmarks];  // metros, espaço do mundo
    float confidence = 0.0f;
    HandGesture gesture = HandGesture::NONE;
    float pinchStrength = 0.0f;   // 0..1
    Vec3 pinchPoint{0, 0, 0};     // ponto entre polegar+indicador
    Vec3 indexTip{0, 0, 0};
    Vec3 palmCenter{0, 0, 0};
    Vec3 velocity{0, 0, 0};       // m/s (para partículas de movimento)
};

} // namespace brazilmr
