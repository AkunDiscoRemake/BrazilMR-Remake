// BrazilMR — detecção de features (FAST-9) e fluxo óptico piramidal (KLT).
// Pipeline do backend de SLAM por CV próprio:
//   FRAME → PRÉ-PROCESSAMENTO (pirâmide) → DETECÇÃO → TRACKING → (pose)
#pragma once

#include "../math/MathTypes.hpp"
#include <cstdint>
#include <vector>

namespace brazilmr {

// Frame de luminância (o canal Y do YUV_420_888, sem cópias extras).
struct LumaView {
    const uint8_t* data = nullptr;
    int width = 0, height = 0, stride = 0;
};

// Pirâmide de imagem com half-sampling (blit 2x2 média).
class ImagePyramid {
public:
    void build(const LumaView& base, int levels);

    LumaView level(int l) const {
        LumaView v;
        if (l < 0 || l >= static_cast<int>(widths_.size())) return v;
        v.data = buffers_[l].data();
        v.width = widths_[l];
        v.height = heights_[l];
        v.stride = strides_[l];
        return v;
    }

    int levels() const { return static_cast<int>(widths_.size()); }
    std::size_t memoryBytes() const {
        std::size_t total = 0;
        for (auto& b : buffers_) total += b.capacity();
        return total;
    }

private:
    std::vector<std::vector<uint8_t>> buffers_;
    std::vector<int> widths_, heights_, strides_;
};

// Coordenada inteira de feature.
struct FeaturePoint {
    float x = 0.0f, y = 0.0f;
    int   score = 0;
};

// FAST-9 com supressão de não-máximos e bucketing em grade.
// @param maxPerCell  features máximas por célula da grade (evita aglomeração)
// @param grid        divisões da grade (grid x grid células)
std::vector<FeaturePoint> detectFeatures(const LumaView& img, int threshold,
                                         int maxFeatures, int grid = 4,
                                         int maxPerCell = 12);

// Feature rastreada pelo KLT.
struct KltTrack {
    float prevX = 0.0f, prevY = 0.0f;
    float curX = 0.0f, curY = 0.0f;
    float error = 0.0f;   // SSD médio no patch
    bool  valid = false;
};

// KLT piramidal (forward-additive, janela 7x7).
// @param[in,out] tracks  posições prevX/prevY devem estar preenchidas;
//                        retorna curX/curY + valid/error.
void trackFeatures(const ImagePyramid& prev, const ImagePyramid& cur,
                   std::vector<KltTrack>& tracks, int iterations = 12,
                   float epsilonPx = 0.03f, float maxSsd = 900.0f);

// Amostragem bilinear (fora dos limites → -1).
float sampleBilinear(const LumaView& img, float x, float y);

} // namespace brazilmr
