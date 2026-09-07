// BrazilMR — câmera VR (uma por olho) + frustum para culling.
#pragma once

#include "../math/MathTypes.hpp"

namespace brazilmr {

struct Camera {
    Mat4 view = Mat4::identity();
    Mat4 proj = Mat4::identity();
    Mat4 viewProj = Mat4::identity();
    Vec3 position{0, 0, 0};
    Quat orientation = Quat::identity();
    float zNear = 0.02f;
    float zFar = 60.0f;

    // Monta a câmera de um olho: pose da cabeça + offset lateral (IPD/2).
    // offsetSign: -1 olho esquerdo, +1 olho direito.
    void setupEye(const Vec3& headPos, const Quat& headOrient, float eyeOffsetM,
                  float fovYRad, float aspect, float zn, float zf) {
        zNear = zn;
        zFar = zf;
        orientation = headOrient;
        position = headPos + headOrient.rotate(Vec3{eyeOffsetM, 0, 0});
        view = Mat4::view(position, orientation);
        proj = Mat4::perspective(fovYRad, aspect, zn, zf);
        viewProj = proj * view;
    }

    // --- frustum culling (planos de viewProj) ---
    // Planos em forma (a,b,c,d) com p·v + d >= 0 dentro.
    float planes[6][4] = {{0}};

    void extractFrustum() {
        const float* m = viewProj.m; // column-major: m[col*4+row]
        auto setPlane = [&](int p, float a, float b, float c, float d) {
            float len = std::sqrt(a * a + b * b + c * c);
            if (len < 1e-9f) len = 1.0f;
            planes[p][0] = a / len; planes[p][1] = b / len;
            planes[p][2] = c / len; planes[p][3] = d / len;
        };
        // row r de viewProj = (m[r], m[4+r], m[8+r], m[12+r])
        // left = row3+row0, right = row3-row0, bottom = row3+row1,
        // top = row3-row1, near = row3+row2, far = row3-row2
        setPlane(0, m[3] + m[0], m[7] + m[4],  m[11] + m[8],  m[15] + m[12]);
        setPlane(1, m[3] - m[0], m[7] - m[4],  m[11] - m[8],  m[15] - m[12]);
        setPlane(2, m[3] + m[1], m[7] + m[5],  m[11] + m[9],  m[15] + m[13]);
        setPlane(3, m[3] - m[1], m[7] - m[5],  m[11] - m[9],  m[15] - m[13]);
        setPlane(4, m[3] + m[2], m[7] + m[6],  m[11] + m[10], m[15] + m[14]);
        setPlane(5, m[3] - m[2], m[7] - m[6],  m[11] - m[10], m[15] - m[14]);
    }

    // AABB no mundo está totalmente fora do frustum?
    bool cullAabb(const Vec3& mn, const Vec3& mx) const {
        for (int p = 0; p < 6; ++p) {
            float a = planes[p][0], b = planes[p][1], c = planes[p][2],
                  d = planes[p][3];
            // vértice mais positivo do AABB na direção do plano
            float px = a >= 0 ? mx.x : mn.x;
            float py = b >= 0 ? mx.y : mn.y;
            float pz = c >= 0 ? mx.z : mn.z;
            if (a * px + b * py + c * pz + d < 0.0f) return true; // fora
        }
        return false;
    }

    // Ray a partir de NDC (para hit-testing de UI espacial).
    // `aspect` entra por compatibilidade de assinatura: o unproject já
    // carrega o aspecto via viewProj (matriz de projeção).
    Vec3 rayForNdc(float ndcX, float ndcY, float aspect, Vec3& dirOut) const {
        (void)aspect; // o aspecto já está embutido em viewProj
        Mat4 invVp = viewProj.inverted();
        Vec3 nearP = invVp.transformPoint({ndcX, ndcY, -1.0f});
        Vec3 farP = invVp.transformPoint({ndcX, ndcY, 1.0f});
        dirOut = (farP - nearP).normalized();
        return nearP;
    }
};

} // namespace brazilmr
