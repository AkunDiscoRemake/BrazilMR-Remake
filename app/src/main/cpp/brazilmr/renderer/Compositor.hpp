// BrazilMR — compositor final.
// Warp do alvo de cada olho para a tela através da malha de distorção:
//   • barrel distortion (k1/k2)
//   • correção de aberração cromática (3 amostras RGB)
//   • vignette
//   • reprojeção "late-latch": rotação extra aplicada entre o fim do render
//     da cena e o swap (reduz latência percebida de rotação)
//   • modo calibração: grade + marcadores de alinhamento de IPD
#pragma once

#include "../BrazilmrConfig.hpp"
#include "../gl/GlUtils.hpp"
#include "DistortionMesh.hpp"
#include "StereoConfig.hpp"
#include "StereoRenderer.hpp"

namespace brazilmr {

class Compositor {
public:
    bool init();
    void destroy();

    // (Re)gera malhas de distorção quando perfil/tela/modo mudam.
    void configure(const StereoProfile& profile, int screenW, int screenH);

    // Desenha os dois olhos na tela.
    //  @param eyeColor0/1  texturas de cor dos alvos (left/right FBO)
    //  @param reproj        matriz 2x2 (tan-space) da correção late-latch
    //  @param timeSec       relógio para efeitos sutis
    void draw(GLuint eyeColorLeft, GLuint eyeColorRight, const float reproj[4],
              float timeSec);

    // Overlay de calibração (grade + círculos de IPD) — desenhado sobre o SBS.
    void drawCalibrationOverlay(const StereoProfile& profile, int screenW,
                                int screenH);

    bool ready() const { return ready_; }

private:
    bool buildMeshes();

    StereoProfile profile_;
    int screenW_ = 0, screenH_ = 0;
    DistortionMesh meshLeft_, meshRight_, meshMono_;
    Mesh calibMesh_;
    Shader warpShader_;
    Shader calibShader_;
    bool ready_ = false;
    bool meshesDirty_ = true;
};

} // namespace brazilmr
