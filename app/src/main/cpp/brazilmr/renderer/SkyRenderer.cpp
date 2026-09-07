#include "SkyRenderer.hpp"

namespace brazilmr {

static const char* kVs = R"(#version 300 es
precision highp float;
out vec2 vNdc;
void main() {
    // triângulo fullscreen
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vNdc = p * 2.0 - 1.0;
    gl_Position = vec4(vNdc, 0.999, 1.0);
}
)";

static const char* kFs = R"(#version 300 es
precision highp float;
in vec2 vNdc;
uniform mat4 uInvViewProj;
uniform float uGrid;
uniform float uTime;
out vec4 outColor;

// ambiente noturno calmo (menos fadiga em telas OLED do VR Box)
const vec3 kTop    = vec3(0.020, 0.031, 0.063);
const vec3 kBottom = vec3(0.008, 0.016, 0.031);
const vec3 kAccent = vec3(0.000, 0.392, 0.431); // verde BrazilMR

void main() {
    vec4 nearP = uInvViewProj * vec4(vNdc, -1.0, 1.0);
    vec4 farP  = uInvViewProj * vec4(vNdc,  1.0, 1.0);
    vec3 dir = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    // gradiente do céu
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 col = mix(kBottom, kTop, pow(t, 0.8));

    // brilho suave no "horizonte" para dar noção de profundidade
    float horizon = exp(-abs(dir.y) * 6.0);
    col += kAccent * horizon * 0.18;

    // grade de chão analítica (y = 0)
    if (dir.y < -0.02) {
        float tFloor = -1.4 / dir.y;              // distância até o plano
        if (tFloor > 0.0 && tFloor < 40.0) {
            vec3 hit = dir * tFloor;
            vec2 g = abs(fract(hit.xz * 0.5) - 0.5) / (fwidth(hit.xz * 0.5) + 1e-5);
            float line = 1.0 - min(min(g.x, g.y), 1.0);
            float fade = clamp(1.0 - tFloor / 40.0, 0.0, 1.0);
            float pulse = 0.75 + 0.25 * sin(uTime * 0.8 - tFloor * 0.15);
            col += kAccent * line * fade * fade * 0.55 * uGrid * pulse;
        }
    }
    outColor = vec4(col, 1.0);
}
)";

bool SkyRenderer::init() {
    if (!shader_.build(kVs, kFs)) return false;
    // triângulo fullscreen via gl_VertexID — sem buffers de vértice
    std::vector<VertexAttrib> noAttrs;
    if (!quad_.upload(nullptr, 3, 1, noAttrs)) return false;
    ready_ = true;
    return true;
}

void SkyRenderer::destroy() {
    shader_.destroy();
    quad_.destroy();
    ready_ = false;
}

void SkyRenderer::draw(const Mat4& invViewProj, float gridIntensity, float timeSec) {
    if (!ready_) return;
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    shader_.use();
    shader_.setMat4("uInvViewProj", invViewProj);
    shader_.setFloat("uGrid", gridIntensity);
    shader_.setFloat("uTime", timeSec);
    quad_.draw(GL_TRIANGLES);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

} // namespace brazilmr
