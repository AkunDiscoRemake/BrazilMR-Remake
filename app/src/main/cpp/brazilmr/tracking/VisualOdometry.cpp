#include "VisualOdometry.hpp"
#include "LinearAlgebra.hpp"
#include <cmath>

namespace brazilmr {

namespace {

// ---------------------------------------------------------------------------
// Utilidades 3x3 (row-major local; conversão onde necessário).
// ---------------------------------------------------------------------------
struct Mat3 {
    float m[9]; // row-major
    static Mat3 identity() { return {{1,0,0, 0,1,0, 0,0,1}}; }
    static Mat3 fromQuat(const Quat& q) {
        Mat4 r = Mat4::rotation(q);
        Mat3 o;
        o.m[0]=r.m[0]; o.m[1]=r.m[4]; o.m[2]=r.m[8];
        o.m[3]=r.m[1]; o.m[4]=r.m[5]; o.m[5]=r.m[9];
        o.m[6]=r.m[2]; o.m[7]=r.m[6]; o.m[8]=r.m[10];
        return o;
    }
    Vec3 apply(const Vec3& v) const {
        return {m[0]*v.x + m[1]*v.y + m[2]*v.z,
                m[3]*v.x + m[4]*v.y + m[5]*v.z,
                m[6]*v.x + m[7]*v.y + m[8]*v.z};
    }
};

// Normalização de Hartley: leva os pontos para centróide 0 e dist. média √2.
struct Normalizer {
    float cx = 0, cy = 0, scale = 1;
};

Normalizer makeNormalizer(const std::vector<Vec2>& pts) {
    Normalizer n;
    if (pts.empty()) return n;
    float mx = 0, my = 0;
    for (auto& p : pts) { mx += p.x; my += p.y; }
    mx /= pts.size(); my /= pts.size();
    float d = 0;
    for (auto& p : pts) {
        float dx = p.x - mx, dy = p.y - my;
        d += std::sqrt(dx * dx + dy * dy);
    }
    d /= pts.size();
    n.cx = mx; n.cy = my;
    n.scale = (d > 1e-9f) ? static_cast<float>(std::sqrt(2.0)) / d : 1.0f;
    return n;
}

inline Vec2 normalizePoint(const Vec2& p, const Normalizer& n) {
    return {(p.x - n.cx) * n.scale, (p.y - n.cy) * n.scale};
}

// Resolve o sistema de 8 pontos: menor autovetor de AᵀA (9x9).
// A tem uma linha por par: coeficientes de E (row-major 3x3) em x2ᵀ E x1 = 0.
bool solveEssential8pt(const std::vector<Vec2>& prevN, const std::vector<Vec2>& curN,
                       float E[9]) {
    if (prevN.size() < 8) return false;
    float AtA[81] = {0};
    for (std::size_t i = 0; i < prevN.size(); ++i) {
        float a[9] = {
            curN[i].x * prevN[i].x, curN[i].x * prevN[i].y, curN[i].x,
            curN[i].y * prevN[i].x, curN[i].y * prevN[i].y, curN[i].y,
            prevN[i].x,             prevN[i].y,             1.0f
        };
        for (int r = 0; r < 9; ++r)
            for (int c = 0; c < 9; ++c) AtA[r * 9 + c] += a[r] * a[c];
    }
    float eig[9], V[81];
    jacobiEigenSymmetric(AtA, 9, eig, V);
    for (int k = 0; k < 9; ++k) E[k] = V[k * 9 + 0]; // menor autovalor → núcleo
    return true;
}

inline float epipolarError(const float E[9], const Vec2& x1, const Vec2& x2) {
    // x2ᵀ E x1 (E row-major)
    float ex[3] = {E[0]*x1.x + E[1]*x1.y + E[2],
                   E[3]*x1.x + E[4]*x1.y + E[5],
                   E[6]*x1.x + E[7]*x1.y + E[8]};
    return ex[0]*x2.x + ex[1]*x2.y + ex[2];
}

// Triangulação DLT: P1=[I|0], P2=[R|t]. Retorna true se profundidades > 0.
bool triangulatePositive(const Mat3& R, const Vec3& t,
                         const Vec2& x1n, const Vec2& x2n) {
    // P1 = [I|0]; P2 = [R|t] → 4 linhas:
    //   [x1*p3⁽¹⁾-p1⁽¹⁾], [y1*p3⁽¹⁾-p2⁽¹⁾], [x2*p3⁽²⁾-p1⁽²⁾], [y2*p3⁽²⁾-p2⁽²⁾]
    float r0[4] = {R.m[0], R.m[1], R.m[2], t.x};
    float r1[4] = {R.m[3], R.m[4], R.m[5], t.y};
    float r2[4] = {R.m[6], R.m[7], R.m[8], t.z};
    float rows[4][4];
    // câmera 1 (P1 = [I|0])
    rows[0][0] = -1.0f; rows[0][1] = 0; rows[0][2] = x1n.x; rows[0][3] = 0;      // x*p3 - p1 = (-1, 0, x, 0)
    rows[1][0] = 0; rows[1][1] = -1.0f; rows[1][2] = x1n.y; rows[1][3] = 0;      // y*p3 - p2
    // câmera 2
    for (int k = 0; k < 4; ++k) {
        rows[2][k] = x2n.x * r2[k] - r0[k];
        rows[3][k] = x2n.y * r2[k] - r1[k];
    }
    float AtA[16] = {0};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            for (int k = 0; k < 4; ++k) AtA[r * 4 + c] += rows[r][k] * rows[c][k];
    float eig[4], V[16];
    jacobiEigenSymmetric(AtA, 4, eig, V);
    float X[4] = {V[0 * 4 + 0], V[1 * 4 + 0], V[2 * 4 + 0], V[3 * 4 + 0]};
    if (std::fabs(X[3]) < 1e-12f) return false;
    float wInv = 1.0f / X[3];
    Vec3 X1{X[0] * wInv, X[1] * wInv, X[2] * wInv};         // no frame da câmera 1
    if (X1.z <= 0.0f) return false;
    Vec3 X2 = R.apply(X1) + t;                               // no frame da câmera 2
    return X2.z > 0.0f;
}

// Kabsch: melhor rotação alinhando raios normalizados prev→cur (rotação pura).
Quat kabschRotation(const std::vector<Vec2>& prevN, const std::vector<Vec2>& curN) {
    float H[9] = {0};
    for (std::size_t i = 0; i < prevN.size(); ++i) {
        float a[3] = {prevN[i].x, prevN[i].y, 1.0f};
        float b[3] = {curN[i].x, curN[i].y, 1.0f};
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c) H[r * 3 + c] += a[r] * b[c]; // H = Σ a bᵀ
    }
    // svd3x3 espera col-major
    float Hcol[9];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) Hcol[c * 3 + r] = H[r * 3 + c];
    float U[9], S[3], V[9];
    if (!svd3x3(Hcol, U, S, V)) return Quat::identity();
    // R = V Uᵀ (aplicando a: b ≈ R a). Em col-major: R_col = U_t? — montamos
    // explicitamente com colunas de V e U.
    // H col-major: H = U S Vᵀ, com U/V colunas em índice [k*3+col].
    float Rcol[9];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) {
            float sum = 0;
            for (int k = 0; k < 3; ++k) sum += V[r * 3 + k] * U[c * 3 + k];
            Rcol[c * 3 + r] = sum; // R = V Uᵀ
        }
    // corrige reflexão
    float det = Rcol[0]*(Rcol[4]*Rcol[8]-Rcol[5]*Rcol[7])
              - Rcol[1]*(Rcol[3]*Rcol[8]-Rcol[5]*Rcol[6])
              + Rcol[2]*(Rcol[3]*Rcol[7]-Rcol[4]*Rcol[6]);
    if (det < 0.0f) {
        for (int r = 0; r < 3; ++r) Rcol[r * 3 + 2] = -Rcol[r * 3 + 2];
    }
    Mat4 M;
    M.m[0]=Rcol[0]; M.m[4]=Rcol[1]; M.m[8]=Rcol[2];
    M.m[1]=Rcol[3]; M.m[5]=Rcol[4]; M.m[9]=Rcol[5];
    M.m[2]=Rcol[6]; M.m[6]=Rcol[7]; M.m[10]=Rcol[8];
    return M.toQuat();
}

} // namespace

// ---------------------------------------------------------------------------
// VisualOdometry
// ---------------------------------------------------------------------------
VoResult VisualOdometry::processFrame(const LumaView& luma, int64_t timestampNs) {
    VoResult res;
    lastTsNs_ = timestampNs;

    curPyr_.build(luma, cfg_.pyramidLevels);

    if (!hasPrev_) {
        auto feats = detectFeatures(curPyr_.level(0), cfg_.fastThreshold,
                                    cfg_.maxFeatures);
        tracks_.clear();
        for (auto& f : feats) {
            KltTrack t;
            t.prevX = t.curX = f.x;
            t.prevY = t.curY = f.y;
            t.valid = true;
            tracks_.push_back(t);
        }
        prevPyr_ = std::move(curPyr_);
        hasPrev_ = true;
        lastFeatures_ = static_cast<int>(tracks_.size());
        return res;
    }

    trackFeatures(prevPyr_, curPyr_, tracks_);

    // pares válidos em coordenadas de pixel
    std::vector<Vec2> prevPts, curPts;
    prevPts.reserve(tracks_.size());
    curPts.reserve(tracks_.size());
    for (auto& t : tracks_) {
        if (t.valid) {
            prevPts.push_back({t.prevX, t.prevY});
            curPts.push_back({t.curX, t.curY});
        }
    }
    res.trackedFeatures = static_cast<int>(prevPts.size());
    lastFeatures_ = res.trackedFeatures;

    if (static_cast<int>(prevPts.size()) >= cfg_.minTracksForPose) {
        Quat rot;
        Vec3 tUnit{0, 0, 0};
        int inliers = 0;
        float err = 0;
        if (estimateMotion(prevPts, curPts, rot, tUnit, inliers, err)) {
            res.ok = true;
            res.rotation = rot;
            res.translationUnit = tUnit;
            res.inliers = inliers;
            res.reprojectionError = err;

            // acumula pose (translação com escala métrica estimada)
            Vec3 stepWorld = voRotation_.rotate(tUnit) * scale_;
            voPosition_ = voPosition_ + stepWorld;
            voRotation_ = (rot * voRotation_).normalized();

            if (cfg_.imuScaleCoupling) {
                voAccum_ = voAccum_ + tUnit;
                voDispWindow_.push_back(stepWorld);
                if (voDispWindow_.size() > 32) voDispWindow_.erase(voDispWindow_.begin());
            }
        } else {
            // rotação pura (sem baseline): Kabsch
            Quat rotOnly = kabschRotation(prevPts, curPts);
            res.ok = true;
            res.rotation = rotOnly;
            res.inliers = static_cast<int>(prevPts.size());
            voRotation_ = (rotOnly * voRotation_).normalized();
        }
    }

    // redeteccão quando o conjunto decai
    bool redetect = static_cast<int>(tracks_.size()) < cfg_.minTracksForPose;
    if (!redetect) {
        int valid = 0;
        for (auto& t : tracks_) if (t.valid) ++valid;
        if (valid < cfg_.minTracksForPose / 2) redetect = true;
    }
    if (redetect) {
        auto feats = detectFeatures(curPyr_.level(0), cfg_.fastThreshold,
                                    cfg_.maxFeatures);
        tracks_.clear();
        for (auto& f : feats) {
            KltTrack t;
            t.prevX = t.curX = f.x;
            t.prevY = t.curY = f.y;
            t.valid = true;
            tracks_.push_back(t);
        }
    } else {
        // promove posições atuais para "anteriores"
        for (auto& t : tracks_) {
            if (t.valid) { t.prevX = t.curX; t.prevY = t.curY; }
        }
        // remove tracks mortas (mantendo orçamento de memória)
        std::vector<KltTrack> alive;
        alive.reserve(tracks_.size());
        for (auto& t : tracks_) if (t.valid) alive.push_back(t);
        tracks_ = std::move(alive);
    }

    prevPyr_ = std::move(curPyr_);
    curPyr_ = ImagePyramid();
    return res;
}

void VisualOdometry::feedImuDisplacement(const Vec3& displacementMeters) {
    if (!cfg_.imuScaleCoupling) return;
    imuAccum_ = imuAccum_ + displacementMeters;
    imuDispWindow_.push_back(displacementMeters);
    if (imuDispWindow_.size() > 32) imuDispWindow_.erase(imuDispWindow_.begin());

    float imuLen = imuAccum_.length();
    float voLen = voAccum_.length();
    if (imuLen > 0.25f && voLen > 0.25f) {
        float s = imuLen / voLen;
        // só atualiza quando há movimento real e plausível
        if (s > 0.02f && s < 50.0f) {
            scale_ = lerpf(scale_, clampf(s, 0.02f, cfg_.maxScale), cfg_.scaleEma);
        }
        imuAccum_ = {0, 0, 0};
        voAccum_ = {0, 0, 0};
    }
    if (imuLen > 4.0f) { // janela exagerada → reseta (drift de acelerômetro)
        imuAccum_ = {0, 0, 0};
        voAccum_ = {0, 0, 0};
    }
}

bool VisualOdometry::estimateMotion(const std::vector<Vec2>& prev,
                                    const std::vector<Vec2>& cur,
                                    Quat& rotOut, Vec3& transUnitOut,
                                    int& inliersOut, float& errOut) {
    const std::size_t n = prev.size();
    if (n < 8) return false;

    // normalização (usando apenas pares com disparidade mínima)
    Normalizer np = makeNormalizer(prev);
    Normalizer nc = makeNormalizer(cur);

    // RANSAC
    const int iters = cfg_.ransacIterations;
    float bestE[9];
    int bestInliers = 0;
    std::vector<unsigned char> bestMask;
    std::vector<Vec2> prevN(n), curN(n);
    for (std::size_t i = 0; i < n; ++i) {
        prevN[i] = normalizePoint(prev[i], np);
        curN[i] = normalizePoint(cur[i], nc);
    }

    // RNG determinístico simples (xorshift) — reprodutível entre sessões.
    uint32_t rngState = static_cast<uint32_t>(lastTsNs_ & 0xFFFFFFFFu) | 1u;
    auto randIndex = [&](std::size_t bound) -> std::size_t {
        rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
        return rngState % bound;
    };

    for (int it = 0; it < iters; ++it) {
        std::vector<Vec2> sp, sc;
        unsigned char seen[8];
        for (int k = 0; k < 8; ++k) {
            std::size_t idx = randIndex(n);
            bool dup = false;
            for (int j = 0; j < k; ++j) if (seen[j] == idx) dup = true;
            if (dup) { --k; continue; }
            seen[k] = static_cast<unsigned char>(idx);
            sp.push_back(prevN[idx]);
            sc.push_back(curN[idx]);
        }
        float E[9];
        if (!solveEssential8pt(sp, sc, E)) continue;

        int inl = 0;
        std::vector<unsigned char> mask(n, 0);
        float inlErr = 0;
        for (std::size_t i = 0; i < n; ++i) {
            float e = std::fabs(epipolarError(E, prevN[i], curN[i]));
            if (e < cfg_.ransacThreshold) {
                mask[i] = 1; ++inl; inlErr += e;
            }
        }
        if (inl > bestInliers) {
            bestInliers = inl;
            std::copy(E, E + 9, bestE);
            bestMask = mask;
        }
    }

    int minInliers = (cfg_.minInlierRatio * static_cast<int>(n)) / 100;
    if (bestInliers < 8 || bestInliers < minInliers) return false;

    // refit com todos os inliers
    {
        std::vector<Vec2> sp, sc;
        for (std::size_t i = 0; i < n; ++i) {
            if (bestMask[i]) { sp.push_back(prevN[i]); sc.push_back(curN[i]); }
        }
        float E[9];
        if (solveEssential8pt(sp, sc, E)) std::copy(E, E + 9, bestE);
    }

    // decomposição: E = U diag(1,1,0) Vᵀ
    float Ecol[9];
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) Ecol[c * 3 + r] = bestE[r * 3 + c];
    float U[9], S[3], V[9];
    if (!svd3x3(Ecol, U, S, V)) return false;

    // W e Wᵀ
    const float W[9] = {0, -1, 0, 1, 0, 0, 0, 0, 1};

    auto mul3 = [&](const float A[9], const float B[9], float Out[9]) {
        // col-major in/out
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r) {
                float sum = 0;
                for (int k = 0; k < 3; ++k) sum += A[k * 3 + r] * B[c * 3 + k];
                Out[c * 3 + r] = sum;
            }
    };

    // candidatos (col-major): R1 = U W Vᵀ, R2 = U Wᵀ Vᵀ
    float R1[9], R2[9], UW[9], UWt[9];
    float Wt[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1}; // Wᵀ
    mul3(U, W, UW);
    mul3(U, Wt, UWt);
    mul3(UW, V, R1);
    mul3(UWt, V, R2);

    Vec3 tU{U[0 * 3 + 2], U[1 * 3 + 2], U[2 * 3 + 2]}; // u₃

    // coleta inliers normalizados para teste de quiralidade
    std::vector<Vec2> inPrev, inCur;
    for (std::size_t i = 0; i < n; ++i) {
        if (bestMask[i]) { inPrev.push_back(prevN[i]); inCur.push_back(curN[i]); }
    }
    int checkCount = static_cast<int>(inPrev.size());
    if (checkCount > 40) checkCount = 40;

    auto tryCandidate = [&](const float R[9], const Vec3& t) -> int {
        // R é col-major → Mat3 row-major exige transposição
        Mat3 Rm = {{R[0], R[3], R[6],
                    R[1], R[4], R[7],
                    R[2], R[5], R[8]}};
        int positive = 0;
        for (int i = 0; i < checkCount; ++i)
            if (triangulatePositive(Rm, t, inPrev[i], inCur[i])) ++positive;
        return positive;
    };

    struct Cand { const float* R; Vec3 t; int score; };
    Cand cands[4] = {
        {R1, tU, -1}, {R1, tU * -1.0f, -1}, {R2, tU, -1}, {R2, tU * -1.0f, -1}
    };
    for (auto& c : cands) c.score = tryCandidate(c.R, c.t);
    int bestCand = 0;
    for (int i = 1; i < 4; ++i) if (cands[i].score > cands[bestCand].score) bestCand = i;

    const float* Rbest = cands[bestCand].R;
    Vec3 tBest = cands[bestCand].t;
    if (tBest.lengthSq() < 1e-12f) return false;

    // valida matriz de rotação (orto) e monta quaternion (Rbest col-major)
    Mat4 M;
    M.m[0]=Rbest[0]; M.m[1]=Rbest[1]; M.m[2]=Rbest[2];
    M.m[4]=Rbest[3]; M.m[5]=Rbest[4]; M.m[6]=Rbest[5];
    M.m[8]=Rbest[6]; M.m[9]=Rbest[7]; M.m[10]=Rbest[8];
    rotOut = M.toQuat().normalized();
    transUnitOut = tBest.normalized();
    inliersOut = bestInliers;

    // erro epipolar médio dos inliers (normalizado)
    float errSum = 0;
    int errCount = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (bestMask[i]) {
            errSum += std::fabs(epipolarError(bestE, prevN[i], curN[i]));
            ++errCount;
        }
    }
    errOut = errCount ? errSum / errCount : 0.0f;
    return true;
}

} // namespace brazilmr
