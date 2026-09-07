#include "OpticalFlow.hpp"

namespace brazilmr {

// ---------------------------------------------------------------------------
// ImagePyramid
// ---------------------------------------------------------------------------
void ImagePyramid::build(const LumaView& base, int levels) {
    if (levels < 1) levels = 1;
    buffers_.resize(levels);
    widths_.resize(levels);
    heights_.resize(levels);
    strides_.resize(levels);

    int w = base.width, h = base.height;
    for (int l = 0; l < levels; ++l) {
        std::size_t need = static_cast<std::size_t>(w) * h;
        buffers_[l].resize(need);
        if (l == 0) {
            // cópia com stride compactado (o buffer vem com stride >= width)
            for (int y = 0; y < h; ++y) {
                const uint8_t* src = base.data + static_cast<std::ptrdiff_t>(y) * base.stride;
                uint8_t* dst = buffers_[0].data() + static_cast<std::ptrdiff_t>(y) * w;
                for (int x = 0; x < w; ++x) dst[x] = src[x];
            }
        } else {
            const uint8_t* src = buffers_[l - 1].data();
            const int pw = widths_[l - 1], ph = heights_[l - 1];
            uint8_t* dst = buffers_[l].data();
            for (int y = 0; y < h; ++y) {
                const int y0 = 2 * y, y1 = (y0 + 1 < ph) ? y0 + 1 : y0;
                for (int x = 0; x < w; ++x) {
                    const int x0 = 2 * x, x1 = (x0 + 1 < pw) ? x0 + 1 : x0;
                    const unsigned a = src[y0 * pw + x0], b = src[y0 * pw + x1];
                    const unsigned c = src[y1 * pw + x0], d = src[y1 * pw + x1];
                    dst[y * w + x] = static_cast<uint8_t>((a + b + c + d + 2) >> 2);
                }
            }
        }
        widths_[l] = w;
        heights_[l] = h;
        strides_[l] = w;
        w = (w + 1) / 2;
        h = (h + 1) / 2;
        if (w < 16 || h < 16) { levels = l + 1; buffers_.resize(levels); widths_.resize(levels); heights_.resize(levels); strides_.resize(levels); break; }
    }
}

// ---------------------------------------------------------------------------
// Amostragem
// ---------------------------------------------------------------------------
float sampleBilinear(const LumaView& img, float x, float y) {
    if (x < 0.0f || y < 0.0f) return -1.0f;
    int w = img.width, h = img.height;
    if (x > static_cast<float>(w - 1) || y > static_cast<float>(h - 1)) return -1.0f;
    int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    int x1 = (x0 + 1 < w) ? x0 + 1 : x0;
    int y1 = (y0 + 1 < h) ? y0 + 1 : y0;
    float fx = x - x0, fy = y - y0;
    const uint8_t* d = img.data;
    float p00 = d[y0 * img.stride + x0], p10 = d[y0 * img.stride + x1];
    float p01 = d[y1 * img.stride + x0], p11 = d[y1 * img.stride + x1];
    return lerpf(lerpf(p00, p10, fx), lerpf(p01, p11, fx), fy);
}

static inline int pixelAt(const LumaView& img, int x, int y) {
    return img.data[y * img.stride + x];
}

// ---------------------------------------------------------------------------
// FAST-9 (segment test) — pixels do círculo em ordem horária.
// ---------------------------------------------------------------------------
static const int kFastCircle16[16][2] = {
    {0, -3}, {1, -3}, {2, -2}, {3, -1}, {3, 0}, {3, 1}, {2, 2}, {1, 3},
    {0, 3}, {-1, 3}, {-2, 2}, {-3, 1}, {-3, 0}, {-3, -1}, {-2, -2}, {-1, -3}
};

static inline int fastScore(const LumaView& img, int x, int y, int center) {
    // soma de |diff| nos 16 vizinhos — score rápido (suficiente p/ NMS)
    int s = 0;
    for (auto& off : kFastCircle16) {
        int v = pixelAt(img, x + off[0], y + off[1]) - center;
        s += v < 0 ? -v : v;
    }
    return s;
}

static inline bool fastCornerAt(const LumaView& img, int x, int y, int threshold) {
    int c = pixelAt(img, x, y);
    // teste rápido: pixels 1, 5, 9, 13
    int d1 = pixelAt(img, x + 3, y) - c;
    int d5 = pixelAt(img, x - 3, y) - c;
    int d9 = pixelAt(img, x, y + 3) - c;
    int d13 = pixelAt(img, x, y - 3) - c;
    int t = threshold;
    // precisa de 3 de 4 acima ou abaixo
    int brighter = (d1 > t) + (d5 > t) + (d9 > t) + (d13 > t);
    int darker = (d1 < -t) + (d5 < -t) + (d9 < -t) + (d13 < -t);
    if (brighter < 3 && darker < 3) return false;
    // segment test completo: 9 contíguos
    int vals[16];
    for (int i = 0; i < 16; ++i)
        vals[i] = pixelAt(img, x + kFastCircle16[i][0], y + kFastCircle16[i][1]) - c;
    for (int start = 0; start < 16; ++start) {
        int runB = 0, runD = 0;
        for (int k = 0; k < 9; ++k) {
            int v = vals[(start + k) & 15];
            if (v > t) ++runB; else runB = 0;
            if (v < -t) ++runD; else runD = 0;
            if (runB >= 9 || runD >= 9) return true;
        }
    }
    return false;
}

std::vector<FeaturePoint> detectFeatures(const LumaView& img, int threshold,
                                         int maxFeatures, int grid, int maxPerCell) {
    std::vector<FeaturePoint> out;
    if (img.width < 32 || img.height < 32 || grid < 1) return out;
    if (maxPerCell < 1) maxPerCell = 1;

    const int border = 4;
    const int cellW = (img.width + grid - 1) / grid;
    const int cellH = (img.height + grid - 1) / grid;
    if (cellW < 8 || cellH < 8) grid = 1;

    // 1) detecta TODOS os candidatos FAST-9
    std::vector<FeaturePoint> corners;
    corners.reserve(256);
    for (int y = border; y < img.height - border; ++y) {
        for (int x = border; x < img.width - border; ++x) {
            if (!fastCornerAt(img, x, y, threshold)) continue;
            int score = fastScore(img, x, y, pixelAt(img, x, y));
            corners.push_back({static_cast<float>(x), static_cast<float>(y), score});
        }
    }

    // 2) bucketing por célula com limite maxPerCell (ordenado por score)
    const int cells = grid * grid;
    std::vector<std::vector<int>> cellIdx(static_cast<std::size_t>(cells));
    const int cw = (img.width + grid - 1) / grid;
    const int ch = (img.height + grid - 1) / grid;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        int gx = static_cast<int>(corners[i].x) / cw;
        int gy = static_cast<int>(corners[i].y) / ch;
        if (gx >= grid) gx = grid - 1;
        if (gy >= grid) gy = grid - 1;
        cellIdx[static_cast<std::size_t>(gy * grid + gx)].push_back(
            static_cast<int>(i));
    }
    std::vector<int> picked;
    picked.reserve(static_cast<std::size_t>(maxFeatures));
    for (auto& cell : cellIdx) {
        // ordena por score (decrescente) e mantém maxPerCell
        for (std::size_t a = 1; a < cell.size(); ++a) {
            int key = cell[a];
            int b = static_cast<int>(a) - 1;
            while (b >= 0 && corners[cell[b]].score < corners[key].score) {
                cell[b + 1] = cell[b];
                --b;
            }
            cell[b + 1] = key;
        }
        for (int k = 0; k < maxPerCell && k < static_cast<int>(cell.size()); ++k)
            picked.push_back(cell[k]);
        if (static_cast<int>(picked.size()) >= maxFeatures) break;
    }

    // 3) ordena por score global e limita
    for (std::size_t a = 1; a < picked.size(); ++a) {
        int key = picked[a];
        int b = static_cast<int>(a) - 1;
        while (b >= 0 && corners[picked[b]].score < corners[key].score) {
            picked[b + 1] = picked[b];
            --b;
        }
        picked[b + 1] = key;
    }
    for (int idx : picked) {
        out.push_back(corners[idx]);
        if (static_cast<int>(out.size()) >= maxFeatures) break;
    }
    return out;
}

// ---------------------------------------------------------------------------
// KLT piramidal
// ---------------------------------------------------------------------------
static const int kHalfWindow = 3; // janela 7x7
static const int kWinDim = kHalfWindow * 2 + 1;

void trackFeatures(const ImagePyramid& prev, const ImagePyramid& cur,
                   std::vector<KltTrack>& tracks, int iterations,
                   float epsilonPx, float maxSsd) {
    const int levels = std::min(prev.levels(), cur.levels());
    if (levels <= 0) {
        for (auto& t : tracks) t.valid = false;
        return;
    }

    for (KltTrack& t : tracks) {
        t.valid = false;
        t.error = 1e9f;
        // guess em pixels do nível base
        float gx = 0.0f, gy = 0.0f; // deslocamento acumulado (nível base)
        bool trackDiverged = false;

        for (int l = levels - 1; l >= 0; --l) {
            const float scaleInv = static_cast<float>(1 << l);
            const float scale = 1.0f / scaleInv;
            // posição no nível l
            float px = t.prevX * scale;
            float py = t.prevY * scale;
            float gxl = gx * scale, gyl = gy * scale;

            LumaView I = prev.level(l); // referência
            LumaView J = cur.level(l);  // atual

            float dx = 0.0f, dy = 0.0f;
            bool diverged = false;

            for (int it = 0; it < iterations; ++it) {
                // montar sistema normal: G = Σ[ Ix² IxIy ; IxIy Iy² ], b = Σ Ix*e, Iy*e
                float Gxx = 0, Gxy = 0, Gyy = 0, bx = 0, by = 0;
                float ssd = 0;
                int count = 0;
                int cx = static_cast<int>(px + gxl + dx + 0.5f);
                int cy = static_cast<int>(py + gyl + dy + 0.5f);
                int rx = static_cast<int>(px + 0.5f);
                int ry = static_cast<int>(py + 0.5f);
                // janela de J (posição candidata) e de I (referência) in-bounds
                if (cx < kHalfWindow + 1 || cy < kHalfWindow + 1 ||
                    cx >= I.width - kHalfWindow - 1 || cy >= I.height - kHalfWindow - 1 ||
                    rx < kHalfWindow || ry < kHalfWindow ||
                    rx >= I.width - kHalfWindow || ry >= I.height - kHalfWindow) {
                    diverged = true; break;
                }
                for (int wy = -kHalfWindow; wy <= kHalfWindow; ++wy) {
                    for (int wx = -kHalfWindow; wx <= kHalfWindow; ++wx) {
                        float sx = px + gxl + dx + wx;
                        float sy = py + gyl + dy + wy;
                        // referência SEMPRE na posição original do nível
                        // (o guess entra apenas na amostragem de J) — senão o
                        // palpite se cancela e o nível resolve o shift completo
                        float iRef = pixelAt(I, static_cast<int>(px + wx + 0.5f),
                                             static_cast<int>(py + wy + 0.5f));
                        float jVal = sampleBilinear(J, sx, sy);
                        if (jVal < 0.0f) { diverged = true; break; }
                        float jx = sampleBilinear(J, sx + 1.0f, sy);
                        float jy = sampleBilinear(J, sx, sy + 1.0f);
                        if (jx < 0.0f || jy < 0.0f) { diverged = true; break; }
                        float ix = jx - jVal;
                        float iy = jy - jVal;
                        float e = iRef - jVal;
                        Gxx += ix * ix; Gxy += ix * iy; Gyy += iy * iy;
                        bx += ix * e;   by += iy * e;
                        ssd += e * e;
                        ++count;
                    }
                    if (diverged) break;
                }
                if (diverged || count < kWinDim * kWinDim / 2) break;

                float det = Gxx * Gyy - Gxy * Gxy;
                if (det < 25.0f) break; // região sem textura
                float stepX = (Gyy * bx - Gxy * by) / det;
                float stepY = (Gxx * by - Gxy * bx) / det;
                dx += stepX;
                dy += stepY;
                if (std::fabs(stepX) < epsilonPx && std::fabs(stepY) < epsilonPx) break;
            }
            if (diverged) { trackDiverged = true; break; } // guess não-confiável

            // propaga o guess para o próximo nível (mais fino);
            // no nível 0 NÃO multiplica (evita dobrar o deslocamento final)
            if (l > 0) {
                gx = (gxl + dx) * 2.0f;
                gy = (gyl + dy) * 2.0f;
            } else {
                gx = gxl + dx;
                gy = gyl + dy;
            }
        }
        if (trackDiverged) continue; // divergiu em algum nível → inválido

        // resultado no nível 0
        float fx = t.prevX + gx;
        float fy = t.prevY + gy;
        if (fx < 0 || fy < 0 || fx >= cur.level(0).width || fy >= cur.level(0).height) continue;

        // erro final: SSD no patch do nível 0
        LumaView I = prev.level(0);
        LumaView J = cur.level(0);
        float ssd = 0.0f;
        int cx = static_cast<int>(fx + 0.5f), cy = static_cast<int>(fy + 0.5f);
        if (cx < kHalfWindow || cy < kHalfWindow || cx >= I.width - kHalfWindow || cy >= I.height - kHalfWindow)
            continue;
        bool ok = true;
        for (int wy = -kHalfWindow; wy <= kHalfWindow && ok; ++wy)
            for (int wx = -kHalfWindow; wx <= kHalfWindow; ++wx) {
                float iRef = pixelAt(I, static_cast<int>(t.prevX + 0.5f) + wx,
                                     static_cast<int>(t.prevY + 0.5f) + wy);
                float jVal = sampleBilinear(J, fx + wx, fy + wy);
                if (jVal < 0.0f) { ok = false; break; }
                float e = iRef - jVal;
                ssd += e * e;
            }
        if (!ok) continue;
        float meanSsd = ssd / static_cast<float>(kWinDim * kWinDim);
        if (meanSsd > maxSsd) continue;

        t.curX = fx;
        t.curY = fy;
        t.error = meanSsd;
        t.valid = true;
    }
}

} // namespace brazilmr
