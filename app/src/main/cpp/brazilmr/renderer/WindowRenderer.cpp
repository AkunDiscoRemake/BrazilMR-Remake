#include "WindowRenderer.hpp"

namespace brazilmr {

// ---------------------------------------------------------------------------
// Shader da janela: curvatura no vertex, SDF de canto no fragment.
// ---------------------------------------------------------------------------
static const char* kWinVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aCorner;   // -1..1
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform float uCurvature;
out vec2 vUv;
void main() {
    // curvatura cilíndrica: recua Z nas bordas horizontais
    float z = -uCurvature * aCorner.x * aCorner.x;
    vec3 local = vec3(aCorner.x, aCorner.y, z);
    vUv = vec2(aCorner.x * 0.5 + 0.5, 0.5 - aCorner.y * 0.5);
    gl_Position = uViewProj * uModel * vec4(local, 1.0);
}
)";

static const char* kWinFs = R"(#version 300 es
#extension GL_OES_EGL_image_external : require
precision highp float;
in vec2 vUv;
uniform samplerExternalOES uContent;
uniform float uHasContent;
uniform float uFocus;       // 0..1
uniform float uTime;
uniform float uAlpha;
uniform float uFxBudget;
uniform vec3 uAccent;
out vec4 outColor;

float roundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    // área útil (deixa margem para a barra de título/borda)
    vec2 p = vUv * 2.0 - 1.0;
    float d = roundedBox(p, vec2(0.985, 0.982), 0.045);
    if (d > 0.0) discard;

    vec3 col;
    float alpha = uAlpha;
    if (uHasContent > 0.5) {
        vec3 tex = texture(uContent, vUv).rgb;
        col = tex;
    } else {
        // placeholder: grade de espera
        vec2 g = abs(fract(vUv * 16.0) - 0.5);
        float line = smoothstep(0.45, 0.5, max(g.x, g.y));
        col = mix(vec3(0.05, 0.06, 0.08), uAccent * 0.25, line * 0.5);
        float pulse = 0.5 + 0.5 * sin(uTime * 2.0);
        col += uAccent * 0.05 * pulse;
    }

    // barra de título sutil no topo
    if (vUv.y < 0.045) {
        col *= 0.55;
        col += uAccent * 0.12;
    }

    // borda de foco
    float border = smoothstep(0.012, 0.0, abs(d + 0.008));
    col += uAccent * border * uFocus * 0.85 * uFxBudget;

    // leve vinheta interna para "afundar" o conteúdo (conforto)
    float r = length(p);
    col *= 1.0 - 0.12 * r * r;

    outColor = vec4(col, alpha);
}
)";

static const char* kRayVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos; // strip: 0=origem, 1=fim
uniform mat4 uViewProj;
uniform vec3 uOrigin;
uniform vec3 uDir;
uniform float uLen;
out float vT;
void main() {
    vT = aPos.x;
    vec3 p = uOrigin + uDir * (uLen * aPos.x);
    // espessura no espaço da tela via offset perpendicular aproximado
    gl_Position = uViewProj * vec4(p, 1.0);
}
)";

static const char* kRayFs = R"(#version 300 es
precision highp float;
in float vT;
uniform vec3 uColor;
uniform float uAlpha;
out vec4 outColor;
void main() {
    float a = uAlpha * (1.0 - vT) * (1.0 - vT);
    outColor = vec4(uColor * (1.2 - vT * 0.6), a);
}
)";

bool WindowRenderer::init() {
    if (!shader_.build(kWinVs, kWinFs)) return false;
    if (!rayShader_.build(kRayVs, kRayFs)) return false;

    // strip do raio: 2 vértices (origem→fim)
    static const float rayVerts[] = {0.0f, 0.0f, 1.0f, 0.0f};
    std::vector<VertexAttrib> rayAttrs = {{2, GL_FLOAT, false}};
    if (!rayMesh_.upload(rayVerts, 2, 8, rayAttrs, nullptr, 0, GL_UNSIGNED_SHORT))
        return false;

    buildQuad();
    ready_ = true;
    return true;
}

void WindowRenderer::destroy() {
    shader_.destroy();
    rayShader_.destroy();
    rayMesh_.destroy();
    quadMesh_.destroy();
    ready_ = false;
    lastQuadCurvature_ = -1.0f;
}

void WindowRenderer::buildQuad() {
    // grid 24x24 para curvatura suave
    const int N = 24;
    std::vector<float> verts;
    std::vector<uint16_t> idx;
    verts.reserve(static_cast<std::size_t>((N + 1) * (N + 1) * 2));
    idx.reserve(static_cast<std::size_t>(N * N * 6));
    for (int j = 0; j <= N; ++j)
        for (int i = 0; i <= N; ++i) {
            verts.push_back(static_cast<float>(i) / N * 2.0f - 1.0f);
            verts.push_back(static_cast<float>(j) / N * 2.0f - 1.0f);
        }
    for (int j = 0; j < N; ++j)
        for (int i = 0; i < N; ++i) {
            uint16_t a = static_cast<uint16_t>(j * (N + 1) + i);
            uint16_t b = a + 1;
            uint16_t c = a + static_cast<uint16_t>(N + 1);
            uint16_t d = c + 1;
            idx.insert(idx.end(), {a, c, b, b, c, d});
        }
    std::vector<VertexAttrib> attrs = {{2, GL_FLOAT, false}};
    quadMesh_.upload(verts.data(), verts.size() / 2, 2 * sizeof(float), attrs,
                     idx.data(), idx.size(), GL_UNSIGNED_SHORT);
}

bool WindowRenderer::ensureWindowMesh(const SpatialWindow& w) {
    // A malha é a mesma; a curvatura é aplicada no shader (uniform).
    (void)w;
    return quadMesh_.vertexCount() > 0;
}

void WindowRenderer::draw(const SpatialWindow& w, const Camera& cam,
                          float timeSec, const EffectQuality& fx) {
    if (!ready_ || !w.visible) return;
    if (!ensureWindowMesh(w)) return;

    Mat4 model = Mat4::translation(w.position) * Mat4::rotation(w.orientation) *
                 Mat4::scale(Vec3{w.widthM * 0.5f * w.scale,
                                  w.heightM * 0.5f * w.scale, 1.0f});
    shader_.use();
    shader_.setMat4("uModel", model);
    shader_.setMat4("uViewProj", cam.viewProj);
    shader_.setFloat("uCurvature", w.curvature);
    shader_.setFloat("uHasContent", w.textureId != 0 ? 1.0f : 0.0f);
    shader_.setFloat("uFocus", w.focused ? 1.0f : 0.25f);
    shader_.setFloat("uTime", timeSec);
    shader_.setFloat("uAlpha", w.alpha);
    shader_.setFloat("uFxBudget", fx.windowFxBudget);
    shader_.setVec3("uAccent", Vec3{0.0f, 0.9f, 0.63f});

    if (w.textureId != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_EXTERNAL_OES, w.textureId);
        shader_.setInt("uContent", 0);
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    quadMesh_.draw();
    glDisable(GL_BLEND);
}

void WindowRenderer::drawPlaceholder(const SpatialWindow& w, const Camera& cam,
                                     float timeSec) {
    SpatialWindow copy = w;
    copy.textureId = 0;
    draw(copy, cam, timeSec, EffectQuality{});
}

void WindowRenderer::drawRay(const Vec3& origin, const Vec3& dir, float lengthM,
                             const Vec3& color, const Camera& cam) {
    if (!ready_) return;
    rayShader_.use();
    rayShader_.setMat4("uViewProj", cam.viewProj);
    rayShader_.setVec3("uOrigin", origin);
    rayShader_.setVec3("uDir", dir.normalized());
    rayShader_.setFloat("uLen", lengthM);
    rayShader_.setVec3("uColor", color);
    rayShader_.setFloat("uAlpha", 0.85f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    rayMesh_.draw(GL_LINES);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

} // namespace brazilmr
