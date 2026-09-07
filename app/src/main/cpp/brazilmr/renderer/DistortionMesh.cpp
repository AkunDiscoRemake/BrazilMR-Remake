#include "DistortionMesh.hpp"

namespace brazilmr {

void buildDistortionGrid(const StereoProfile& profile, int eye, int grid,
                         int screenW, int screenH, bool fullScreen,
                         std::vector<DistortionVertex>& outVerts,
                         std::vector<uint16_t>& outIndices) {
    outVerts.clear();
    outIndices.clear();
    if (grid < 2 || screenW < 2 || screenH < 2) return;

    Rect rects[2];
    computeEyeRects(screenW, screenH, rects[0], rects[1]);
    const Rect& r = fullScreen ? Rect{0, 0, screenW, screenH} : rects[eye & 1];

    // Espaço da lente: unidade = meia-largura do retângulo do olho.
    // px ∈ [-1..1] horizontal, py ∈ [-1..1] vertical (proporcional).
    const float aspect = static_cast<float>(r.w) / static_cast<float>(r.h);
    const float cx = profile.lensCenterX * 2.0f - 1.0f;   // [-1..1]
    const float cy = profile.lensCenterY * 2.0f - 1.0f;

    // Coeficientes por canal para correção cromática:
    // vermelho distorce mais, azul menos (dispersão da lente real).
    const float ca = clampf(profile.chromaticAberration, 0.0f, 1.0f) * 0.02f;
    const float k1R = profile.lensK1 * (1.0f + ca);
    const float k2R = profile.lensK2 * (1.0f + ca);
    const float k1G = profile.lensK1;
    const float k2G = profile.lensK2;
    const float k1B = profile.lensK1 * (1.0f - ca);
    const float k2B = profile.lensK2 * (1.0f - ca);

    outVerts.reserve(static_cast<std::size_t>((grid + 1) * (grid + 1)));
    outIndices.reserve(static_cast<std::size_t>(grid * grid * 6));

    for (int j = 0; j <= grid; ++j) {
        for (int i = 0; i <= grid; ++i) {
            float u = static_cast<float>(i) / static_cast<float>(grid);  // 0..1 no retângulo
            float v = static_cast<float>(j) / static_cast<float>(grid);

            // posição de tela em clip space
            float ndcX = (static_cast<float>(r.x) + u * r.w) * 2.0f / screenW - 1.0f;
            float ndcY = 1.0f - (static_cast<float>(r.y) + v * r.h) * 2.0f / screenH;

            // coordenadas da lente (aspect-corrigidas)
            float px = (u * 2.0f - 1.0f - cx) * aspect;
            float py = (v * 2.0f - 1.0f - cy);
            float rScreen = std::sqrt(px * px + py * py);

            // raio na textura por canal
            float rR = undistortRadius(rScreen, k1R, k2R);
            float rG = undistortRadius(rScreen, k1G, k2G);
            float rB = undistortRadius(rScreen, k1B, k2B);
            float scaleG = (rScreen > 1e-6f) ? rG / rScreen : 1.0f;
            float scaleR = (rScreen > 1e-6f) ? rR / rScreen : 1.0f;
            float scaleB = (rScreen > 1e-6f) ? rB / rScreen : 1.0f;

            // UV na textura do olho (0..1): volta ao espaço do retângulo
            auto toUV = [&](float sx, float sy) {
                float ex = cx + sx / aspect;   // [-1..1] no retângulo
                float ey = cy + sy;
                return Vec2{(ex + 1.0f) * 0.5f, (ey + 1.0f) * 0.5f};
            };
            Vec2 uvR = toUV(px * scaleR, py * scaleR);
            Vec2 uvG = toUV(px * scaleG, py * scaleG);
            Vec2 uvB = toUV(px * scaleB, py * scaleB);

            // clamp — áreas fora da lente ficam na borda (vignette mascara)
            auto cl = [](Vec2 uv) {
                return Vec2{clampf(uv.x, 0.0f, 1.0f), clampf(uv.y, 0.0f, 1.0f)};
            };
            uvR = cl(uvR); uvG = cl(uvG); uvB = cl(uvB);

            DistortionVertex dv;
            dv.x = ndcX;
            dv.y = ndcY;
            dv.uR = uvR.x; dv.vR = uvR.y;
            dv.uG = uvG.x; dv.vG = uvG.y;
            dv.uB = uvB.x; dv.vB = uvB.y;
            outVerts.push_back(dv);
        }
    }

    for (int j = 0; j < grid; ++j) {
        for (int i = 0; i < grid; ++i) {
            std::uint16_t a = static_cast<std::uint16_t>(j * (grid + 1) + i);
            std::uint16_t b = a + 1;
            std::uint16_t c = a + static_cast<std::uint16_t>(grid + 1);
            std::uint16_t d = c + 1;
            outIndices.push_back(a); outIndices.push_back(c); outIndices.push_back(b);
            outIndices.push_back(b); outIndices.push_back(c); outIndices.push_back(d);
        }
    }
}

bool DistortionMesh::build(const StereoProfile& profile, int eye, int grid,
                           int screenW, int screenH, bool fullScreen) {
    buildDistortionGrid(profile, eye, grid, screenW, screenH, fullScreen,
                        verts_, indices_);
    if (verts_.empty()) return false;
    std::vector<VertexAttrib> attrs = {
        {2, GL_FLOAT, false},   // pos
        {2, GL_FLOAT, false},   // uvR
        {2, GL_FLOAT, false},   // uvG
        {2, GL_FLOAT, false},   // uvB
    };
    return mesh_.upload(verts_.data(), verts_.size(), sizeof(DistortionVertex),
                        attrs, indices_.data(), indices_.size(), GL_UNSIGNED_SHORT);
}

void DistortionMesh::destroy() {
    mesh_.destroy();
    verts_.clear();
    indices_.clear();
}

void DistortionMesh::draw() const {
    mesh_.draw(GL_TRIANGLES);
}

} // namespace brazilmr
