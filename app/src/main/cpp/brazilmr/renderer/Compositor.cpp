#include "Compositor.hpp"

namespace brazilmr {

static const char* kWarpVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUvR;
layout(location = 2) in vec2 aUvG;
layout(location = 3) in vec2 aUvB;
out vec2 vUvR;
out vec2 vUvG;
out vec2 vUvB;
void main() {
    vUvR = aUvR;
    vUvG = aUvG;
    vUvB = aUvB;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* kWarpFs = R"(#version 300 es
precision highp float;
in vec2 vUvR;
in vec2 vUvG;
in vec2 vUvB;
uniform sampler2D uEye;
uniform vec2 uTanHalfFov;    // (tan(fovX/2), tan(fovY/2))
uniform mat2 uReproj;        // late-latch: rotação em tan-space
uniform float uVignette;
out vec4 outColor;

void main() {
    // reprojeção em tan-space (pequenas rotações desde o render da cena)
    vec2 pG = (vUvG * 2.0 - 1.0) * uTanHalfFov;
    vec2 pR = (vUvR * 2.0 - 1.0) * uTanHalfFov;
    vec2 pB = (vUvB * 2.0 - 1.0) * uTanHalfFov;
    pG = uReproj * pG;
    pR = uReproj * pR;
    pB = uReproj * pB;
    vec2 uvG = pG / uTanHalfFov * 0.5 + 0.5;
    vec2 uvR = pR / uTanHalfFov * 0.5 + 0.5;
    vec2 uvB = pB / uTanHalfFov * 0.5 + 0.5;

    // fora do alvo → preto (bordas do warp)
    if (any(lessThan(uvG, vec2(0.0))) || any(greaterThan(uvG, vec2(1.0)))) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    float r = texture(uEye, uvR).r;
    vec2 g = texture(uEye, uvG).rg;
    float b = texture(uEye, uvB).b;
    vec3 col = vec3(r, g.g, b);

    // vignette radial suave (queda de brilho nas bordas das lentes)
    vec2 c = vUvG - vec2(0.5, 0.5);
    float vig = 1.0 - uVignette * dot(c, c) * 2.4;
    outColor = vec4(col * clamp(vig, 0.0, 1.0), 1.0);
}
)";

// grade de calibração + círculos de alinhamento de IPD
static const char* kCalibVs = R"(#version 300 es
precision highp float;
out vec2 vNdc;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vNdc = p * 2.0 - 1.0;
    gl_Position = vec4(vNdc, 0.0, 1.0);
}
)";

static const char* kCalibFs = R"(#version 300 es
precision highp float;
in vec2 vNdc;
uniform float uEyeSign;   // -1 esquerdo, +1 direito
out vec4 outColor;
void main() {
    // grade linear
    vec2 g = abs(fract(vNdc * 10.0) - 0.5) / (fwidth(vNdc * 10.0) + 1e-5);
    float line = 1.0 - min(min(g.x, g.y), 1.0);

    // círculo: deve fundir em UM círculo quando o IPD está certo
    float d = length(vNdc - vec2(uEyeSign * 0.35, 0.0));
    float circle = 1.0 - smoothstep(0.010, 0.012, abs(d - 0.22));
    float dotC = 1.0 - smoothstep(0.006, 0.010, d);

    vec3 col = vec3(0.9, 0.95, 1.0) * line * 0.35;
    col += vec3(0.0, 0.9, 0.63) * circle;
    col += vec3(1.0, 0.3, 0.2) * dotC;
    outColor = vec4(col, 1.0);
}
)";

bool Compositor::init() {
    if (!warpShader_.build(kWarpVs, kWarpFs)) return false;
    if (!calibShader_.build(kCalibVs, kCalibFs)) return false;
    std::vector<VertexAttrib> noAttrs;
    if (!calibMesh_.upload(nullptr, 3, 1, noAttrs)) return false;
    ready_ = true;
    meshesDirty_ = true;
    return true;
}

void Compositor::destroy() {
    warpShader_.destroy();
    calibShader_.destroy();
    meshLeft_.destroy();
    meshRight_.destroy();
    meshMono_.destroy();
    calibMesh_.destroy();
    ready_ = false;
    meshesDirty_ = true;
}

void Compositor::configure(const StereoProfile& profile, int screenW,
                           int screenH) {
    if (profile_.lensK1 != profile.lensK1 ||
        profile_.lensK2 != profile.lensK2 ||
        profile_.chromaticAberration != profile.chromaticAberration ||
        profile_.lensCenterX != profile.lensCenterX ||
        profile_.lensCenterY != profile.lensCenterY ||
        screenW_ != screenW || screenH_ != screenH ||
        profile_.sbsMode != profile.sbsMode) {
        meshesDirty_ = true;
    }
    profile_ = profile;
    screenW_ = screenW;
    screenH_ = screenH;
}

bool Compositor::buildMeshes() {
    bool ok = meshLeft_.build(profile_, 0, limits::kDistortionGrid, screenW_,
                              screenH_, false);
    ok = ok && meshRight_.build(profile_, 1, limits::kDistortionGrid, screenW_,
                                screenH_, false);
    ok = ok && meshMono_.build(profile_, 0, limits::kDistortionGrid, screenW_,
                               screenH_, true);
    meshesDirty_ = !ok;
    return ok;
}

void Compositor::draw(GLuint eyeColorLeft, GLuint eyeColorRight,
                      const float reproj[4], float timeSec) {
    if (!ready_) return;
    if (meshesDirty_ && !buildMeshes()) return;

    Framebuffer::bindDefault(screenW_, screenH_);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    warpShader_.use();

    float fovY = profile_.eyeFovYRad();
    Rect l, r;
    computeEyeRects(screenW_, screenH_, l, r);
    float aspect = static_cast<float>(l.w) / static_cast<float>(l.h);
    float tanX = std::tan(fovY * 0.5f) * aspect;
    float tanY = std::tan(fovY * 0.5f);
    warpShader_.setVec2("uTanHalfFov", Vec2{tanX, tanY});
    warpShader_.setFloat("uVignette", profile_.vignette);
    GLint loc = warpShader_.uniform("uReproj");
    const float ident[4] = {1, 0, 0, 1};
    const float* rep = reproj ? reproj : ident;
    if (loc >= 0) glUniformMatrix2fv(loc, 1, GL_FALSE, rep);
    (void)timeSec;

    glActiveTexture(GL_TEXTURE0);
    warpShader_.setInt("uEye", 0);
    glViewport(0, 0, screenW_, screenH_);

    const bool swap = profile_.swapEyes;
    switch (profile_.sbsMode) {
        case SbsMode::NORMAL:
        case SbsMode::INVERTED: {
            // INVERTED = lentes fisicamente trocadas: espelha de novo
            const bool flip = swap ^ (profile_.sbsMode == SbsMode::INVERTED);
            GLuint leftTex = flip ? eyeColorRight : eyeColorLeft;
            GLuint rightTex = flip ? eyeColorLeft : eyeColorRight;
            glBindTexture(GL_TEXTURE_2D, leftTex);
            meshLeft_.draw();
            glBindTexture(GL_TEXTURE_2D, rightTex);
            meshRight_.draw();
            break;
        }
        case SbsMode::LEFT_ONLY: {
            glBindTexture(GL_TEXTURE_2D, swap ? eyeColorRight : eyeColorLeft);
            meshMono_.draw();
            break;
        }
        case SbsMode::RIGHT_ONLY: {
            glBindTexture(GL_TEXTURE_2D, swap ? eyeColorLeft : eyeColorRight);
            meshMono_.draw();
            break;
        }
        case SbsMode::CALIBRATION:
            drawCalibrationOverlay(profile_, screenW_, screenH_);
            break;
    }
    glEnable(GL_DEPTH_TEST);
}

void Compositor::drawCalibrationOverlay(const StereoProfile& profile,
                                        int screenW, int screenH) {
    Framebuffer::bindDefault(screenW, screenH);
    glDisable(GL_DEPTH_TEST);
    calibShader_.use();

    glViewport(0, 0, screenW / 2, screenH);
    calibShader_.setFloat("uEyeSign", -1.0f);
    calibMesh_.draw(GL_TRIANGLES);

    glViewport(screenW / 2, 0, screenW - screenW / 2, screenH);
    calibShader_.setFloat("uEyeSign", 1.0f);
    calibMesh_.draw(GL_TRIANGLES);
    (void)profile;
}

} // namespace brazilmr
