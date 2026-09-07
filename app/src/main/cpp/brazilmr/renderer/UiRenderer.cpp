#include "UiRenderer.hpp"

namespace brazilmr {

// ---------------------------------------------------------------------------
// Shaders — quads no espaço do painel (x,y em metros, origem topo-esquerda).
// ---------------------------------------------------------------------------
static const char* kRectVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;    // metros no painel (origem topo-esq, +y p/ baixo)
layout(location = 1) in vec4 aRect;   // x,y,w,h do rect (para SDF)
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aParams; // radius, border, hover, kind
uniform mat4 uViewProj;
uniform mat4 uPanel;
uniform vec2 uOrigin;                 // w/2, h/2 do painel
out vec2 vLocal;
out vec4 vRect;
out vec4 vColor;
out vec4 vParams;
void main() {
    vec3 local = vec3(aPos.x - uOrigin.x, uOrigin.y - aPos.y, 0.0);
    gl_Position = uViewProj * uPanel * vec4(local, 1.0);
    vLocal = aPos;
    vRect = aRect;
    vColor = aColor;
    vParams = aParams;
}
)";

static const char* kRectFs = R"(#version 300 es
precision highp float;
in vec2 vLocal;
in vec4 vRect;
in vec4 vColor;
in vec4 vParams;
out vec4 outColor;
void main() {
    vec2 p = vLocal - (vRect.xy + vRect.zw * 0.5);
    vec2 half = vRect.zw * 0.5;
    float r = vParams.x;
    vec2 q = abs(p) - half + r;
    float d = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
    if (d > 0.0) discard;
    float aa = fwidth(d) + 1e-4;
    float fill = smoothstep(aa, -aa, d);

    // borda/contorno
    float border = smoothstep(aa, -aa, abs(d + vParams.y)) * step(0.001, vParams.y);

    // hover: brilho
    float hover = vParams.z;

    vec3 col = vColor.rgb;
    col += vec3(0.0, 0.9, 0.63) * hover * 0.16;
    outColor = vec4(col, vColor.a * fill);
    if (border > 0.0) {
        outColor.rgb = mix(outColor.rgb, vec3(0.0, 0.9, 0.63), border * (0.35 + 0.5 * hover));
    }
}
)";

static const char* kTextVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat4 uViewProj;
uniform mat4 uPanel;
out vec2 vUv;
out vec4 vColor;
void main() {
    gl_Position = uViewProj * uPanel * vec4(aPos, 0.0, 1.0);
    vUv = aUv;
    vColor = aColor;
}
)";

static const char* kTextFs = R"(#version 300 es
precision highp float;
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uAtlas;
out vec4 outColor;
void main() {
    float a = texture(uAtlas, vUv).r;
    outColor = vec4(vColor.rgb, vColor.a * a);
}
)";

static const char* kIconVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;
uniform mat4 uViewProj;
uniform mat4 uPanel;
out vec2 vUv;
out vec4 vColor;
void main() {
    gl_Position = uViewProj * uPanel * vec4(aPos, 0.0, 1.0);
    vUv = aUv;
    vColor = aColor;
}
)";

static const char* kIconFs = R"(#version 300 es
precision highp float;
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uIcon;
out vec4 outColor;
void main() {
    vec4 t = texture(uIcon, vUv);
    outColor = t * vColor;
}
)";

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
bool UiRenderer::init() {
    if (!rectShader_.build(kRectVs, kRectFs)) return false;
    if (!textShader_.build(kTextVs, kTextFs)) return false;
    if (!iconShader_.build(kIconVs, kIconFs)) return false;
    ensureDynamicBuffers();
    ready_ = true;
    return true;
}

void UiRenderer::destroy() {
    rectShader_.destroy();
    textShader_.destroy();
    iconShader_.destroy();
    if (rectVao_) glDeleteVertexArrays(1, &rectVao_);
    if (rectVbo_) glDeleteBuffers(1, &rectVbo_);
    if (textVao_) glDeleteVertexArrays(1, &textVao_);
    if (textVbo_) glDeleteBuffers(1, &textVbo_);
    if (iconVao_) glDeleteVertexArrays(1, &iconVao_);
    if (iconVbo_) glDeleteBuffers(1, &iconVbo_);
    rectVao_ = rectVbo_ = textVao_ = textVbo_ = iconVao_ = iconVbo_ = 0;
    ready_ = false;
}

void UiRenderer::ensureDynamicBuffers() {
    // rect: pos2 + rect4 + color4 + params4 = 14 floats
    glGenVertexArrays(1, &rectVao_);
    glGenBuffers(1, &rectVbo_);
    glBindVertexArray(rectVao_);
    glBindBuffer(GL_ARRAY_BUFFER, rectVbo_);
    int stride = 14 * sizeof(float);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));
    for (int i = 0; i < 4; ++i) glEnableVertexAttribArray(i);

    // text/icon: pos2 + uv2 + color4 = 8 floats
    for (int which = 0; which < 2; ++which) {
        GLuint* vao = which == 0 ? &textVao_ : &iconVao_;
        GLuint* vbo = which == 0 ? &textVbo_ : &iconVbo_;
        glGenVertexArrays(1, vao);
        glGenBuffers(1, vbo);
        glBindVertexArray(*vao);
        glBindBuffer(GL_ARRAY_BUFFER, *vbo);
        int s = 8 * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, s, (void*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, s, (void*)(2 * sizeof(float)));
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, s, (void*)(4 * sizeof(float)));
        for (int i = 0; i < 3; ++i) glEnableVertexAttribArray(i);
    }
    glBindVertexArray(0);
}

void UiRenderer::drawRectQuads(const Camera& cam, const Mat4& panelMat,
                               const Vec2& origin, const std::vector<float>& quads) {
    if (quads.empty()) return;
    glBindVertexArray(rectVao_);
    glBindBuffer(GL_ARRAY_BUFFER, rectVbo_);
    glBufferData(GL_ARRAY_BUFFER, quads.size() * sizeof(float), quads.data(),
                 GL_DYNAMIC_DRAW);
    rectShader_.setMat4("uViewProj", cam.viewProj);
    rectShader_.setMat4("uPanel", panelMat);
    rectShader_.setVec2("uOrigin", origin);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quads.size() / 14));
    glBindVertexArray(0);
}

void UiRenderer::drawTextQuads(const Camera& cam, const Mat4& panelMat,
                               const Vec2& origin, const std::vector<float>& quads) {
    if (quads.empty() || !atlas_.valid()) return;
    glBindVertexArray(textVao_);
    glBindBuffer(GL_ARRAY_BUFFER, textVbo_);
    glBufferData(GL_ARRAY_BUFFER, quads.size() * sizeof(float), quads.data(),
                 GL_DYNAMIC_DRAW);
    textShader_.setMat4("uViewProj", cam.viewProj);
    textShader_.setMat4("uPanel", panelMat);
    textShader_.setVec2("uOrigin", origin);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas_.texture());
    textShader_.setInt("uAtlas", 0);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quads.size() / 8));
    glBindVertexArray(0);
}

void UiRenderer::drawIconQuads(const Camera& cam, const Mat4& panelMat,
                               const Vec2& origin, const std::vector<float>& quads,
                               GLuint texId) {
    if (quads.empty() || !texId) return;
    glBindVertexArray(iconVao_);
    glBindBuffer(GL_ARRAY_BUFFER, iconVbo_);
    glBufferData(GL_ARRAY_BUFFER, quads.size() * sizeof(float), quads.data(),
                 GL_DYNAMIC_DRAW);
    iconShader_.setMat4("uViewProj", cam.viewProj);
    iconShader_.setMat4("uPanel", panelMat);
    iconShader_.setVec2("uOrigin", origin);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texId);
    iconShader_.setInt("uIcon", 0);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quads.size() / 8));
    glBindVertexArray(0);
}

// helper local: adiciona rect quad (6 vértices × 14 floats)
static void pushRect(std::vector<float>& out, float x, float y, float w, float h,
                     float radius, float border, float hover,
                     const float color[4], int kind) {
    float x1 = x + w, y1 = y + h;
    float corners[6][2] = {{x, y}, {x1, y}, {x1, y1}, {x, y}, {x1, y1}, {x, y1}};
    for (auto& c : corners) {
        out.push_back(c[0]);
        out.push_back(c[1]);
        out.push_back(x); out.push_back(y); out.push_back(w); out.push_back(h);
        out.push_back(color[0]); out.push_back(color[1]);
        out.push_back(color[2]); out.push_back(color[3]);
        out.push_back(radius);
        out.push_back(border);
        out.push_back(hover);
        out.push_back(static_cast<float>(kind));
    }
}

void UiRenderer::drawPanel(const UiPanel& p, const Camera& cam,
                           const EffectQuality& fx, float timeSec) {
    Mat4 panelMat = Mat4::translation(p.position) * Mat4::rotation(p.orientation) *
                    Mat4::scale(Vec3{1.0f, 1.0f, 1.0f});

    rectVerts_.clear();
    textVerts_.clear();
    iconVerts_.clear();
    GLuint iconTex = 0;

    // fundo do painel
    float back[4] = {0.055f, 0.063f, 0.082f, 0.92f};
    pushRect(rectVerts_, 0, 0, p.widthM, p.heightM, 0.035f, 0.003f, 0.0f, back, 0);

    // barra de título
    if (p.hasTitle) {
        float title[4] = {0.03f, 0.04f, 0.055f, 1.0f};
        pushRect(rectVerts_, 0, 0, p.widthM, 0.075f, 0.035f, 0.0f, 0.0f, title, 0);
        float white[4] = {0.92f, 0.95f, 0.97f, 1.0f};
        atlas_.buildQuads(p.title, 0.03f, 0.055f, 0.045f, white, textVerts_);
    }

    const float textWhite[4] = {0.92f, 0.95f, 0.97f, 1.0f};
    const float textDim[4] = {0.62f, 0.68f, 0.75f, 1.0f};
    const float accent[4] = {0.0f, 0.9f, 0.63f, 1.0f};

    for (const auto& c : p.controls) {
        if (!c.visible) continue;
        float hover = c.hovered ? 1.0f : 0.0f;
        float press = c.pressed ? 1.0f : 0.0f;

        switch (c.kind) {
            case UiControlKind::BUTTON: {
                float col[4] = {0.10f + press * 0.06f, 0.12f + press * 0.07f,
                                0.16f + press * 0.09f, 0.98f};
                pushRect(rectVerts_, c.x, c.y, c.w, c.h, 0.018f,
                         0.0025f, hover, col, 1);
                // label centralizado
                float th = std::fmin(c.h * 0.5f, 0.038f);
                float tw = atlas_.measureText(c.label, th);
                atlas_.buildQuads(c.label, c.x + (c.w - tw) * 0.5f,
                                  c.y + c.h * 0.5f + th * 0.36f, th, textWhite,
                                  textVerts_);
                break;
            }
            case UiControlKind::SLIDER: {
                // trilho
                float track[4] = {0.12f, 0.14f, 0.18f, 1.0f};
                float trackY = c.y + c.h * 0.5f - 0.008f;
                pushRect(rectVerts_, c.x, trackY, c.w, 0.016f, 0.008f, 0.0f, 0.0f,
                         track, 2);
                // preenchimento
                float fill[4] = {0.0f, 0.55f, 0.40f, 1.0f};
                pushRect(rectVerts_, c.x, trackY, c.w * c.value, 0.016f, 0.008f,
                         0.0f, hover * 0.5f, fill, 2);
                // knob
                float knob[4] = {0.85f, 0.92f, 0.95f, 1.0f};
                float kx = c.x + c.w * c.value - 0.022f;
                pushRect(rectVerts_, kx, c.y + c.h * 0.5f - 0.028f, 0.044f, 0.056f,
                         0.022f, 0.0f, hover, knob, 3);
                // label + valor
                atlas_.buildQuads(c.label, c.x, c.y + 0.024f, 0.03f, textDim, textVerts_);
                char val[32];
                std::snprintf(val, sizeof(val), "%d%%",
                              static_cast<int>(c.value * 100.0f + 0.5f));
                float vw = atlas_.measureText(val, 0.03f);
                atlas_.buildQuads(val, c.x + c.w - vw, c.y + 0.024f, 0.03f,
                                  accent, textVerts_);
                break;
            }
            case UiControlKind::LABEL: {
                atlas_.buildQuads(c.label, c.x, c.y + c.h * 0.72f,
                                  std::fmin(c.h * 0.7f, 0.042f), textDim, textVerts_);
                break;
            }
            case UiControlKind::TOGGLE: {
                float bgOn[4] = {0.0f, 0.45f, 0.33f, 1.0f};
                float bgOff[4] = {0.12f, 0.14f, 0.18f, 1.0f};
                const float* bg = c.value > 0.5f ? bgOn : bgOff;
                float w = c.w * 0.42f, h = 0.045f;
                pushRect(rectVerts_, c.x, c.y + c.h * 0.5f - h * 0.5f, w, h, h * 0.5f,
                         0.0f, hover, const_cast<float*>(bg), 4);
                float kx = c.x + (c.value > 0.5f ? w - h * 0.92f : h * 0.08f);
                float knob[4] = {0.95f, 0.97f, 1.0f, 1.0f};
                pushRect(rectVerts_, kx, c.y + c.h * 0.5f - h * 0.42f,
                         h * 0.84f, h * 0.84f, h * 0.42f, 0.0f, 0.0f, knob, 4);
                float th = std::fmin(c.h * 0.5f, 0.036f);
                atlas_.buildQuads(c.label, c.x + w + 0.025f, c.y + c.h * 0.5f + th * 0.36f,
                                  th, textWhite, textVerts_);
                break;
            }
            case UiControlKind::ICON: {
                if (c.iconTexture) {
                    iconTex = c.iconTexture;
                    float x1 = c.x + c.w, y1 = c.y + c.h;
                    float uv[4] = {c.iconUv[0], c.iconUv[1], c.iconUv[2], c.iconUv[3]};
                    float corners[6][2] = {{c.x, c.y}, {x1, c.y}, {x1, y1},
                                           {c.x, c.y}, {x1, y1}, {c.x, y1}};
                    float uvs[6][2] = {{uv[0], uv[3]}, {uv[2], uv[3]}, {uv[2], uv[1]},
                                       {uv[0], uv[3]}, {uv[2], uv[1]}, {uv[0], uv[1]}};
                    for (int k = 0; k < 6; ++k) {
                        iconVerts_.push_back(corners[k][0]);
                        iconVerts_.push_back(corners[k][1]);
                        iconVerts_.push_back(uvs[k][0]);
                        iconVerts_.push_back(uvs[k][1]);
                        iconVerts_.push_back(1); iconVerts_.push_back(1);
                        iconVerts_.push_back(1); iconVerts_.push_back(1);
                    }
                    if (c.hovered) {
                        float gl[4] = {0.0f, 0.9f, 0.63f, 0.25f};
                        pushRect(rectVerts_, c.x - 0.012f, c.y - 0.012f,
                                 c.w + 0.024f, c.h + 0.024f, 0.02f, 0.0f, 1.0f, gl, 5);
                    }
                }
                float th = 0.028f;
                float tw = atlas_.measureText(c.label, th);
                atlas_.buildQuads(c.label, c.x + (c.w - tw) * 0.5f,
                                  c.y + c.h + th * 1.2f, th, textDim, textVerts_);
                break;
            }
        }
    }

    (void)timeSec; (void)fx;
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    Vec2 origin{p.widthM * 0.5f, p.heightM * 0.5f};
    rectShader_.use();
    drawRectQuads(cam, panelMat, origin, rectVerts_);
    textShader_.use();
    drawTextQuads(cam, panelMat, origin, textVerts_);
    if (iconTex) {
        iconShader_.use();
        drawIconQuads(cam, panelMat, origin, iconVerts_, iconTex);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void UiRenderer::drawScene(UiScene& scene, const Camera& cam,
                           const EffectQuality& fx, float timeSec) {
    if (!ready_) return;
    for (auto& p : scene.panels()) {
        if (!p.visible) continue;
        drawPanel(p, cam, fx, timeSec);
    }
}

} // namespace brazilmr
