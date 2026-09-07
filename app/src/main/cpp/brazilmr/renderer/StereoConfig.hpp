// BrazilMR — configuração estéreo do headset (IPD, FOV, lentes, escala).
// Tudo aqui é ajustável no painel de calibração VR.
#pragma once

#include "../BrazilmrConfig.hpp"
#include "../math/MathTypes.hpp"

namespace brazilmr {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

struct StereoProfile {
    // FOV vertical por olho (graus). Típico VR Box: 80–100.
    float eyeFovYDeg = 92.0f;
    // Distância interpupilar (mm). Típico: 58–70. Calibrável.
    float ipdMm = 63.5f;
    // Distorção de lente (barrel): r' = r (1 + k1 r² + k2 r⁴)
    float lensK1 = 0.22f;
    float lensK2 = 0.20f;
    // Força da correção de aberração cromática (0 = off, 1 = máxima).
    float chromaticAberration = 0.6f;
    // Vignette (0 = off .. 1 = forte)
    float vignette = 0.30f;
    // Escala de renderização interna (resolução do alvo por olho vs meia-tela).
    float renderScale = 0.8f;
    // Troca esquerda↔direita (lentes invertidas / montagem espelhada).
    bool swapEyes = false;
    SbsMode sbsMode = SbsMode::NORMAL;
    // Deslocamento do centro óptico da lente (fração da meia-tela; VR Boxes
    // reais têm lentes levemente acima do centro).
    float lensCenterX = 0.5f;  // horizontal dentro do retângulo do olho
    float lensCenterY = 0.5f;
    // Tela física (para calibração em metros / conforto).
    float screenPhysicalWidthMm = 140.0f;
    float screenPhysicalHeightMm = 70.0f;

    float ipdMeters() const { return ipdMm * 0.001f; }
    float eyeFovYRad() const { return degToRad(eyeFovYDeg); }
};

// Retângulos de cada olho na tela (landscape, SBS).
// left = x∈[0,w/2), right = x∈[w/2,w). Com swapEyes os UVs trocam no
// compositor — os retângulos continuam os mesmos.
inline void computeEyeRects(int screenW, int screenH, Rect& left, Rect& right) {
    left.x = 0;
    left.y = 0;
    left.w = screenW / 2;
    left.h = screenH;
    right.x = screenW / 2;
    right.y = 0;
    right.w = screenW - left.w;
    right.h = screenH;
}

} // namespace brazilmr
