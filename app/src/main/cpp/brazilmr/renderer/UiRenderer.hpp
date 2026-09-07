// BrazilMR — renderizador da UI espacial.
// Painéis com cantos arredondados, botões, sliders, toggles, ícones e texto
// (atlas). Buffers dinâmicos reutilizados — zero alocação por frame.
#pragma once

#include "../BrazilmrConfig.hpp"
#include "../gl/GlUtils.hpp"
#include "../ui/UiScene.hpp"
#include "Camera.hpp"

namespace brazilmr {

class UiRenderer {
public:
    bool init();
    void destroy();

    void setTextAtlas(const TextAtlas& atlas) { atlas_ = atlas; }
    TextAtlas& textAtlas() { return atlas_; }

    // Desenha todos os painéis visíveis da cena.
    void drawScene(UiScene& scene, const Camera& cam, const EffectQuality& fx,
                   float timeSec);

private:
    void drawPanel(const UiPanel& p, const Camera& cam, const EffectQuality& fx,
                   float timeSec);
    void ensureDynamicBuffers();
    void drawRectQuads(const Camera& cam, const Mat4& panelMat, const Vec2& origin,
                       const std::vector<float>& quads);
    void drawTextQuads(const Camera& cam, const Mat4& panelMat, const Vec2& origin,
                       const std::vector<float>& quads);
    void drawIconQuads(const Camera& cam, const Mat4& panelMat, const Vec2& origin,
                       const std::vector<float>& quads, GLuint texId);

    // shaders
    Shader rectShader_;   // rounded rect + hover/press + slider track/knob
    Shader textShader_;   // atlas texturizado
    Shader iconShader_;   // textura de ícone (app icons)

    // VBO/VAO dinâmicos
    GLuint rectVao_ = 0, rectVbo_ = 0;
    GLuint textVao_ = 0, textVbo_ = 0;
    GLuint iconVao_ = 0, iconVbo_ = 0;

    // buffers de construção reutilizados
    std::vector<float> rectVerts_;
    std::vector<float> textVerts_;
    std::vector<float> iconVerts_;

    TextAtlas atlas_;
    bool ready_ = false;
};

} // namespace brazilmr
