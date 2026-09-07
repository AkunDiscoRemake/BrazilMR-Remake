// BrazilMR — ambiente: skybox com gradiente + grade de chão analítica.
// Um único draw call fullscreen; o shader reconstrói o raio por pixel.
#pragma once

#include "../gl/GlUtils.hpp"

namespace brazilmr {

class SkyRenderer {
public:
    bool init();
    void destroy();
    // Desenha antes de todo o resto (sem depth write).
    // @param invViewProj  inversa de (proj*view) da câmera do olho
    void draw(const Mat4& invViewProj, float gridIntensity, float timeSec);

private:
    Shader shader_;
    Mesh quad_;
    bool ready_ = false;
};

} // namespace brazilmr
