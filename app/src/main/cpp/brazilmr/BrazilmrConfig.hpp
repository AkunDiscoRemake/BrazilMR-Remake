// BrazilMR — configuração de compilação e limites globais do runtime nativo.
#pragma once

#include <cstddef>

namespace brazilmr {

// ---------------------------------------------------------------------------
// Limites de memória (o telefone vive dentro de um VR Box: memória é crítica).
// ---------------------------------------------------------------------------
namespace limits {
    // Malha de distorção por olho (vertices = GRID*GRID).
    constexpr int   kDistortionGrid          = 48;
    // Máximo de janelas espaciais simultâneas.
    constexpr int   kMaxSpatialWindows       = 8;
    // Máximo de nós de cena.
    constexpr int   kMaxSceneNodes           = 2048;
    // Landmarks por mão (MediaPipe Hands = 21).
    constexpr int   kHandLandmarks           = 21;
    // Features de odometria visual rastreadas simultaneamente.
    constexpr int   kMaxVoFeatures           = 160;
    // Níveis de pirâmide do KLT.
    constexpr int   kKltPyramidLevels        = 3;
    // Buffer circular de amostras de IMU para predição.
    constexpr int   kImuHistorySize          = 128;
    // Atlas de fonte: glifos máximos.
    constexpr int   kMaxGlyphs               = 512;
    // Partículas do sistema de efeitos das mãos (leve por projeto).
    constexpr int   kMaxHandParticles        = 96;
    // Ripples espaciais simultâneos (feedback de clique).
    constexpr int   kMaxRipples              = 16;
    // Texturas externas (janelas/superfícies de apps).
    constexpr int   kMaxExternalTextures     = 16;
} // namespace limits

// ---------------------------------------------------------------------------
// Qualidades de efeito — escaladas pelo Thermal/Performance Manager.
// ---------------------------------------------------------------------------
struct EffectQuality {
    float handGlow        = 1.0f;   // intensidade do glow das mãos
    float handParticles   = 1.0f;   // fração de partículas ativas
    float windowFxBudget  = 1.0f;   // bordas/glow de janelas
    float skyDetail       = 1.0f;   // detalhe do grid do ambiente
};

// ---------------------------------------------------------------------------
// Modos SBS suportados.
// ---------------------------------------------------------------------------
enum class SbsMode : int {
    NORMAL        = 0,  // esquerda | direita
    INVERTED      = 1,  // direita  | esquerda (lentes trocadas)
    LEFT_ONLY     = 2,  // fullscreen com o olho esquerdo
    RIGHT_ONLY    = 3,  // fullscreen com o olho direito
    CALIBRATION   = 4,  // padrão de calibração (grid + marcadores IPD)
};

} // namespace brazilmr
