// BrazilMR — renderização de janelas espaciais.
// Quad levemente curvado + shader com cantos arredondados, barra de título,
// glow de foco e amostragem da textura externa (SurfaceTexture OES).
#pragma once

#include "../BrazilmrConfig.hpp"
#include "../gl/GlUtils.hpp"
#include "../windows/SpatialWindow.hpp"
#include "Camera.hpp"
#include <vector>

namespace brazilmr {

class WindowRenderer {
public:
    bool init();
    void destroy();

    // Desenha uma janela (textura externa via SurfaceTexture já vinculada
    // pela camada Java com glGenTextures no mesmo contexto).
    void draw(const SpatialWindow& w, const Camera& cam, float timeSec,
              const EffectQuality& fx);

    // Placeholder quando não há textura (grade + ícone).
    void drawPlaceholder(const SpatialWindow& w, const Camera& cam, float timeSec);

    // Ray visual de controller/mão (linha fina com gradiente).
    void drawRay(const Vec3& origin, const Vec3& dir, float lengthM,
                 const Vec3& color, const Camera& cam);

private:
    void buildQuad();
    bool ensureWindowMesh(const SpatialWindow& w);

    Shader shader_;
    Shader rayShader_;
    Mesh rayMesh_;
    bool ready_ = false;
    float lastQuadCurvature_ = -1.0f;
    Mesh quadMesh_;
};

} // namespace brazilmr
