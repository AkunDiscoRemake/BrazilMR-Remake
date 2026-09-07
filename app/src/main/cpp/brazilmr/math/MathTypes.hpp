// BrazilMR — tipos matemáticos do runtime (float, single precision, SIMD-friendly).
// Matrizes são column-major, compatíveis direto com OpenGL ES.
#pragma once

#include <cmath>
#include <cstdint>

namespace brazilmr {

constexpr float kPi     = 3.14159265358979323846f;
constexpr float kPiHalf = 1.57079632679489661923f;
constexpr float kEps    = 1e-6f;

inline float degToRad(float d) { return d * (kPi / 180.0f); }
inline float radToDeg(float r) { return r * (180.0f / kPi); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }

// ===========================================================================
// Vec2
// ===========================================================================
struct Vec2 {
    float x = 0.0f, y = 0.0f;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
};

// ===========================================================================
// Vec3
// ===========================================================================
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3() = default;
    Vec3(float s) : x(s), y(s), z(s) {}
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { float inv = 1.0f / s; return {x * inv, y * inv, z * inv}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    bool operator==(const Vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Vec3& o) const { return !(*this == o); }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(const Vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    float lengthSq() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSq()); }
    Vec3 normalized() const {
        float len = length();
        if (len < kEps) return {0, 0, 0};
        return *this / len;
    }
    static Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
        return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)};
    }
};

inline Vec3 operator*(float s, const Vec3& v) { return v * s; }

// ===========================================================================
// Vec4
// ===========================================================================
struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

// ===========================================================================
// Quaternion (x, y, z, w) — convenção do Android SensorManager.
// ===========================================================================
struct Quat {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
    Quat() = default;
    Quat(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}

    static Quat identity() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    static Quat fromAxisAngle(const Vec3& axis, float angleRad) {
        Vec3 a = axis.normalized();
        float h = 0.5f * angleRad;
        float s = std::sin(h);
        return {a.x * s, a.y * s, a.z * s, std::cos(h)};
    }

    // Convenção VR: q = qy(yaw) * qx(pitch) * qz(roll)
    // (aplica roll, depois pitch, depois yaw — como headsets/aeroespacial).
    static Quat fromEulerYXZ(float yawRad, float pitchRad, float rollRad) {
        Quat qy = {0.0f, std::sin(yawRad * 0.5f), 0.0f, std::cos(yawRad * 0.5f)};
        Quat qx = {std::sin(pitchRad * 0.5f), 0.0f, 0.0f, std::cos(pitchRad * 0.5f)};
        Quat qz = {0.0f, 0.0f, std::sin(rollRad * 0.5f), std::cos(rollRad * 0.5f)};
        return (qy * (qx * qz)).normalized();
    }

    // Decomposição em yaw/pitch/roll (recíproca exata de fromEulerYXZ).
    // R = Ry*Rx*Rz → yaw=atan2(R02,R22), pitch=asin(-R12), roll=atan2(R10,R11)
    void toEulerYXZ(float& yawRad, float& pitchRad, float& rollRad) const {
        // entradas da matriz de rotação direto do quaternion:
        // R02=2(xz+wy), R22=1-2(x²+y²), R12=2(yz-wx), R10=2(xy+wz), R11=1-2(x²+z²)
        yawRad   = std::atan2(2.0f * (x * z + w * y), 1.0f - 2.0f * (x * x + y * y));
        pitchRad = std::asin(clampf(2.0f * (w * x - y * z), -1.0f, 1.0f));
        rollRad  = std::atan2(2.0f * (x * y + w * z), 1.0f - 2.0f * (x * x + z * z));
    }

    Quat operator*(const Quat& o) const {
        return {
            w * o.x + x * o.w + y * o.z - z * o.y,
            w * o.y - x * o.z + y * o.w + z * o.x,
            w * o.z + x * o.y - y * o.x + z * o.w,
            w * o.w - x * o.x - y * o.y - z * o.z
        };
    }

    float lengthSq() const { return x * x + y * y + z * z + w * w; }
    float length() const { return std::sqrt(lengthSq()); }
    Quat normalized() const {
        float len = length();
        if (len < kEps) return identity();
        float inv = 1.0f / len;
        return {x * inv, y * inv, z * inv, w * inv};
    }
    Quat conjugate() const { return {-x, -y, -z, w}; }
    Quat inverse() const { return conjugate().normalized(); }

    // Rotação de um vetor.
    Vec3 rotate(const Vec3& v) const {
        // v' = q * (0,v) * q* — forma otimizada:
        Vec3 u{x, y, z};
        Vec3 t = u.cross(v) * 2.0f;
        return v + t * w + u.cross(t);
    }

    Vec3 toEulerVec() const {
        float y, p, r;
        toEulerYXZ(y, p, r);
        return {p, y, r}; // (pitch, yaw, roll)
    }

    static Quat slerp(const Quat& a, const Quat& b, float t) {
        float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        Quat end = b;
        if (dot < 0.0f) { end = {-b.x, -b.y, -b.z, -b.w}; dot = -dot; }
        if (dot > 0.9995f) { // quase paralelos → nlerp
            Quat r{
                lerpf(a.x, end.x, t), lerpf(a.y, end.y, t),
                lerpf(a.z, end.z, t), lerpf(a.w, end.w, t)};
            return r.normalized();
        }
        float theta0 = std::acos(clampf(dot, -1.0f, 1.0f));
        float theta  = theta0 * t;
        float s0 = std::cos(theta) - dot * std::sin(theta) / std::sin(theta0);
        float s1 = std::sin(theta) / std::sin(theta0);
        return {
            a.x * s0 + end.x * s1, a.y * s0 + end.y * s1,
            a.z * s0 + end.z * s1, a.w * s0 + end.w * s1};
    }
};

// Logaritmo de quaternion: devolve vetor de rotação (eixo*ângulo) da rotação q.
// Válido para rotações |ângulo| < 180° (suficiente para filtros de tracking).
inline Vec3 quatLog(const Quat& q) {
    Quat n = q.normalized();
    Vec3 v{n.x, n.y, n.z};
    float vLen = v.length();
    if (vLen < kEps) return {0, 0, 0};
    float angle = 2.0f * std::atan2(vLen, n.w);
    return v * (angle / vLen);
}

// Exponencial (eixo*ângulo → quaternion).
inline Quat quatExp(const Vec3& rotationVec) {
    float angle = rotationVec.length();
    if (angle < kEps) return Quat::identity();
    return Quat::fromAxisAngle(rotationVec, angle);
}

// ===========================================================================
// Mat4 — column-major (m[col * 4 + row]), como OpenGL espera.
// ===========================================================================
struct Mat4 {
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    static Mat4 identity() { return Mat4(); }

    static Mat4 translation(const Vec3& t) {
        Mat4 r;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }

    static Mat4 rotation(const Quat& q) {
        Mat4 r;
        float x = q.x, y = q.y, z = q.z, w = q.w;
        float x2 = x + x, y2 = y + y, z2 = z + z;
        float xx = x * x2, xy = x * y2, xz = x * z2;
        float yy = y * y2, yz = y * z2, zz = z * z2;
        float wx = w * x2, wy = w * y2, wz = w * z2;
        r.m[0] = 1.0f - (yy + zz);  r.m[1] = xy + wz;         r.m[2]  = xz - wy;
        r.m[4] = xy - wz;           r.m[5] = 1.0f - (xx + zz); r.m[6]  = yz + wx;
        r.m[8] = xz + wy;           r.m[9] = yz - wx;          r.m[10] = 1.0f - (xx + yy);
        return r;
    }

    // Projeção perspectiva padrão GL (depth [0,1], olhar -Z).
    static Mat4 perspective(float fovYRad, float aspect, float zNear, float zFar) {
        Mat4 r;
        float f = 1.0f / std::tan(fovYRad * 0.5f);
        r.m[0]  = f / aspect;
        r.m[5]  = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;
        r.m[14] = 2.0f * zFar * zNear / (zNear - zFar);
        r.m[15] = 0.0f;
        return r;
    }

    // View matrix a partir da pose (posição + orientação) da câmera.
    static Mat4 view(const Vec3& position, const Quat& orientation) {
        Mat4 r = rotation(orientation);
        // view = R^T * T(-p)
        Mat4 t = translation(-position);
        return r.transposed() * t;
    }

    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k)
                    sum += m[k * 4 + row] * o.m[c * 4 + k];
                r.m[c * 4 + row] = sum;
            }
        }
        return r;
    }

    Mat4 transposed() const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row)
                r.m[row * 4 + c] = m[c * 4 + row];
        return r;
    }

    Vec3 transformPoint(const Vec3& p) const {
        return {
            m[0] * p.x + m[4] * p.y + m[8]  * p.z + m[12],
            m[1] * p.x + m[5] * p.y + m[9]  * p.z + m[13],
            m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]
        };
    }

    Vec3 transformDir(const Vec3& d) const {
        return {
            m[0] * d.x + m[4] * d.y + m[8]  * d.z,
            m[1] * d.x + m[5] * d.y + m[9]  * d.z,
            m[2] * d.x + m[6] * d.y + m[10] * d.z
        };
    }

    // Extração da translação.
    Vec3 translation() const { return {m[12], m[13], m[14]}; }

    // Inversão geral (Gauss-Jordan). Adequada para uso fora do hot path.
    Mat4 inverted() const {
        Mat4 r;
        float inv[16], det;
        const float* a = m;

        inv[0]  =  a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inv[4]  = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inv[8]  =  a[4] * a[9]  * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inv[12] = -a[4] * a[9]  * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
        inv[1]  = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inv[5]  =  a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inv[9]  = -a[0] * a[9]  * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inv[13] =  a[0] * a[9]  * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
        inv[2]  =  a[1] * a[6]  * a[15] - a[1] * a[7]  * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] + a[13] * a[2] * a[7]  - a[13] * a[3] * a[6];
        inv[6]  = -a[0] * a[6]  * a[15] + a[0] * a[7]  * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] - a[12] * a[2] * a[7]  + a[12] * a[3] * a[6];
        inv[10] =  a[0] * a[5]  * a[15] - a[0] * a[7]  * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] + a[12] * a[1] * a[7]  - a[12] * a[3] * a[5];
        inv[14] = -a[0] * a[5]  * a[14] + a[0] * a[6]  * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] - a[12] * a[1] * a[6]  + a[12] * a[2] * a[5];
        inv[3]  = -a[1] * a[6]  * a[11] + a[1] * a[7]  * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] - a[9]  * a[2] * a[7]  + a[9]  * a[3] * a[6];
        inv[7]  =  a[0] * a[6]  * a[11] - a[0] * a[7]  * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] + a[8]  * a[2] * a[7]  - a[8]  * a[3] * a[6];
        inv[11] = -a[0] * a[5]  * a[11] + a[0] * a[7]  * a[9]  + a[4] * a[1] * a[11] - a[4] * a[3] * a[9]  - a[8]  * a[1] * a[7]  + a[8]  * a[3] * a[5];
        inv[15] =  a[0] * a[5]  * a[10] - a[0] * a[6]  * a[9]  - a[4] * a[1] * a[10] + a[4] * a[2] * a[9]  + a[8]  * a[1] * a[6]  - a[8]  * a[2] * a[5];

        det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        if (std::fabs(det) < 1e-12f) return identity();
        float invDet = 1.0f / det;
        for (int i = 0; i < 16; ++i) r.m[i] = inv[i] * invDet;
        return r;
    }

    // Rotação (3x3 superior) → quaternion.
    Quat toQuat() const {
        float trace = m[0] + m[5] + m[10];
        Quat q;
        if (trace > 0.0f) {
            float s = 0.5f / std::sqrt(trace + 1.0f);
            q.w = 0.25f / s;
            q.x = (m[6] - m[9])  * s;
            q.y = (m[8] - m[2])  * s;
            q.z = (m[1] - m[4])  * s;
        } else if (m[0] > m[5] && m[0] > m[10]) {
            float s = 2.0f * std::sqrt(1.0f + m[0] - m[5] - m[10]);
            q.w = (m[6] - m[9])  / s;
            q.x = 0.25f * s;
            q.y = (m[4] + m[1])  / s;
            q.z = (m[8] + m[2])  / s;
        } else if (m[5] > m[10]) {
            float s = 2.0f * std::sqrt(1.0f + m[5] - m[0] - m[10]);
            q.w = (m[8] - m[2])  / s;
            q.x = (m[4] + m[1])  / s;
            q.y = 0.25f * s;
            q.z = (m[9] + m[6])  / s;
        } else {
            float s = 2.0f * std::sqrt(1.0f + m[10] - m[0] - m[5]);
            q.w = (m[1] - m[4])  / s;
            q.x = (m[8] + m[2])  / s;
            q.y = (m[9] + m[6])  / s;
            q.z = 0.25f * s;
        }
        return q.normalized();
    }
};

// Interseção raio-plano (para hit-testing de janelas/painéis espaciais).
// Retorna t (distância ao longo do raio) ou -1.
inline float rayPlane(const Vec3& origin, const Vec3& dir,
                      const Vec3& planePoint, const Vec3& planeNormal) {
    float d = dir.dot(planeNormal);
    if (std::fabs(d) < kEps) return -1.0f;
    float t = (planePoint - origin).dot(planeNormal) / d;
    return t >= 0.0f ? t : -1.0f;
}

} // namespace brazilmr
