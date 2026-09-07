// BrazilMR — malha de distorção da lente (barrel + aberração cromática).
// A geração é 100% matemática pura (testável no host); o upload é uma
// operação simples de VAO.
#pragma once

#include "../gl/GlUtils.hpp"
#include "StereoConfig.hpp"
#include <cstdint>
#include <vector>

namespace brazilmr {

// Vértice da malha de warp: posição em clip space + UV por canal (R/G/B).
// O shader do compositor amostra a textura do olho 3x (uma por canal).
struct DistortionVertex {
    float x, y;         // clip space [-1..1]
    float uR, vR;
    float uG, vG;
    float uB, vB;
};

// Resolve a distorção inversa: dado o raio na TELA (rScreen, espaço da lente
// normalizado), devolve o raio na TEXTURA do olho. Newton-Raphson.
inline float undistortRadius(float rScreen, float k1, float k2) {
    if (rScreen < 1e-6f) return 0.0f;
    float r = rScreen;                     // chute inicial
    for (int i = 0; i < 8; ++i) {
        float r2 = r * r;
        float f = r * (1.0f + k1 * r2 + k2 * r2 * r2) - rScreen;
        float fp = 1.0f + 3.0f * k1 * r2 + 5.0f * k2 * r2 * r2;
        if (fp < 1e-6f) break;
        float step = f / fp;
        r -= step;
        if (r < 0.0f) r = 0.0f;
        if (std::fabs(step) < 1e-5f) break;
    }
    return r;
}

// Fator de distorção direto (para testes/calibração).
inline float distortRadius(float rTexture, float k1, float k2) {
    float r2 = rTexture * rTexture;
    return rTexture * (1.0f + k1 * r2 + k2 * r2 * r2);
}

// Gera a malha de warp de um olho.
//  @param eye         0 = esquerdo (retângulo esquerdo da tela), 1 = direito
//  @param grid        subdivisões (N x N quads)
//  @param fullScreen  quando true, a malha cobre a tela INTEIRA (modos mono)
void buildDistortionGrid(const StereoProfile& profile, int eye, int grid,
                         int screenW, int screenH, bool fullScreen,
                         std::vector<DistortionVertex>& outVerts,
                         std::vector<uint16_t>& outIndices);

// Malha pronta para uso GL.
class DistortionMesh {
public:
    bool build(const StereoProfile& profile, int eye, int grid,
               int screenW, int screenH, bool fullScreen = false);
    void draw() const;
    void destroy();
    bool valid() const { return mesh_.vertexCount() > 0; }
    std::size_t vertexCount() const { return mesh_.vertexCount(); }

    // Última malha gerada (para testes de host).
    const std::vector<DistortionVertex>& debugVerts() const { return verts_; }

private:
    Mesh mesh_;
    std::vector<DistortionVertex> verts_;
    std::vector<uint16_t> indices_;
};

} // namespace brazilmr
