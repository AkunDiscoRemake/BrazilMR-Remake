// BrazilMR — filtros de Kalman.
// 1) KalmanPosVel: 2 estados [posição, velocidade] por eixo — modelo de
//    velocidade constante; usado para suavizar/mostrar posição 6DoF.
// 2) OrientationEkf: filtro de erro (attitude + bias de giroscópio) 6 estados,
//    medição = quaternion da fusão IMU; usado para suavização fina + predição.
#pragma once

#include "../math/MathTypes.hpp"

namespace brazilmr {

// ---------------------------------------------------------------------------
// Kalman escalar 2x2: estado [p, v], medição p.
// ---------------------------------------------------------------------------
struct KalmanPosVelConfig {
    float processNoiseAccel = 2.0f;   // Q: variância de aceleração (m/s²)²
    float measurementNoise  = 0.003f; // R: variância da medição (m²)
};

class KalmanPosVel {
public:
    using Config = KalmanPosVelConfig;

    KalmanPosVel() : cfg_() { reset(0.0f); }
    explicit KalmanPosVel(const Config& cfg) : cfg_(cfg) { reset(0.0f); }

    void configure(const Config& cfg) { cfg_ = cfg; }

    void reset(float p0) {
        x_[0] = p0; x_[1] = 0.0f;
        P_[0][0] = 1.0f; P_[0][1] = 0.0f;
        P_[1][0] = 0.0f; P_[1][1] = 1.0f;
    }

    // Predição por dt segundos.
    void predict(float dt) {
        if (dt <= 0.0f) return;
        // x = F x ; F = [[1, dt],[0,1]]
        x_[0] += x_[1] * dt;
        // P = F P F^T + Q ; Q = q * [ [dt^4/4, dt^3/2], [dt^3/2, dt^2] ]
        float q = cfg_.processNoiseAccel;
        float p00 = P_[0][0] + dt * (P_[1][0] + P_[0][1]) + dt * dt * P_[1][1];
        float p01 = P_[0][1] + dt * P_[1][1];
        float p10 = P_[1][0] + dt * P_[1][1];
        float p11 = P_[1][1];
        P_[0][0] = p00 + q * (dt * dt * dt * dt / 4.0f);
        P_[0][1] = p01 + q * (dt * dt * dt / 2.0f);
        P_[1][0] = p10 + q * (dt * dt * dt / 2.0f);
        P_[1][1] = p11 + q * (dt * dt);
    }

    // Correção com medição de posição.
    void update(float z) {
        float r = cfg_.measurementNoise;
        // inovação
        float y = z - x_[0];
        // S = H P H^T + R = P00 + r
        float s = P_[0][0] + r;
        // K = P H^T / S = [P00, P10] / s
        float k0 = P_[0][0] / s;
        float k1 = P_[1][0] / s;
        // x += K y
        x_[0] += k0 * y;
        x_[1] += k1 * y;
        // P = (I - K H) P
        float p00 = (1.0f - k0) * P_[0][0];
        float p01 = (1.0f - k0) * P_[0][1];
        float p10 = P_[1][0] - k1 * P_[0][0];
        float p11 = P_[1][1] - k1 * P_[0][1];
        P_[0][0] = p00; P_[0][1] = p01; P_[1][0] = p10; P_[1][1] = p11;
    }

    float position() const { return x_[0]; }
    float velocity() const { return x_[1]; }
    float positionVariance() const { return P_[0][0]; }

private:
    Config cfg_;
    float x_[2];
    float P_[2][2];
};

// Três eixos: posição 3D completa.
struct KalmanPos3 {
    KalmanPosVel x, y, z;
    KalmanPos3() = default;
    explicit KalmanPos3(const KalmanPosVelConfig& cfg) : x(cfg), y(cfg), z(cfg) {}

    void configure(const KalmanPosVel::Config& cfg) { x.configure(cfg); y.configure(cfg); z.configure(cfg); }
    void reset(const Vec3& p0) { x.reset(p0.x); y.reset(p0.y); z.reset(p0.z); }
    void predict(float dt) { x.predict(dt); y.predict(dt); z.predict(dt); }
    void update(const Vec3& pos) { x.update(pos.x); y.update(pos.y); z.update(pos.z); }
    Vec3 position() const { return {x.position(), y.position(), z.position()}; }
    Vec3 velocity() const { return {x.velocity(), y.velocity(), z.velocity()}; }
    Vec3 predictAhead(float dt) const {
        return {x.position() + x.velocity() * dt,
                y.position() + y.velocity() * dt,
                z.position() + z.velocity() * dt};
    }
};

// ---------------------------------------------------------------------------
// Filtro de orientação por espaço de erro: estado de erro e = [δatt(3), δbias(3)],
// nominal = quaternion integrado do giroscópio. Medição = quaternion da fusão
// complementar (ou ARCore). Predição rotacional para frente no tempo usando ω.
// ---------------------------------------------------------------------------
struct OrientationEkfConfig {
    float gyroNoiseRadSec = 0.002f; // densidade de ruído do giroscópio
    float gyroBiasNoise   = 1e-5f;  // estabilidade do bias (rad/s²)²
    float measNoiseRad    = 0.02f;  // ruído da medição de atitude (rad²)
};

class OrientationEkf {
public:
    using Config = OrientationEkfConfig;

    OrientationEkf() : cfg_() { reset(); }
    explicit OrientationEkf(const Config& cfg) : cfg_(cfg) { reset(); }

    void configure(const Config& cfg) { cfg_ = cfg; }
    void reset() {
        q_ = Quat::identity();
        bias_ = {0, 0, 0};
        for (int i = 0; i < 6; ++i) P_[i][i] = 0.01f;
    }

    // Integração nominal do giroscópio (rad/s) por dt.
    void integrateGyro(const Vec3& gyroRadSec, float dt) {
        if (dt <= 0.0f) return;
        Vec3 w = gyroRadSec - bias_;
        float angle = w.length();
        if (angle > kEps) {
            Quat dq = Quat::fromAxisAngle(w, angle * dt);
            q_ = (dq * q_).normalized();
        }
        // propaga covariância do erro (aprox. linear): F = [[I, -dt I],[0, I]]
        float qAtt = cfg_.gyroNoiseRadSec * cfg_.gyroNoiseRadSec * dt;
        float qBias = cfg_.gyroBiasNoise * cfg_.gyroBiasNoise * dt;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j)
                P_[i][j] -= dt * P_[i + 3][j];
            P_[i][i] += qAtt;
        }
        for (int i = 3; i < 6; ++i) {
            for (int j = 0; j < 6; ++j)
                P_[i][j] += (i == j) ? qBias : 0.0f;
        }
        // mantém simetria (aproximação numérica)
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j) {
                float s = 0.5f * (P_[i][j] + P_[j][i]);
                P_[i][j] = s; P_[j][i] = s;
            }
    }

    // Correção com atitude medida (quaternion) — atualização conjunta 3-eixo.
    void updateAttitude(const Quat& qMeas) {
        // erro de rotação: r = log(q_meas * q_nominal^-1)
        Quat qErr = qMeas * q_.conjugate();
        if (qErr.w < 0.0f) qErr = {-qErr.x, -qErr.y, -qErr.z, -qErr.w};
        Vec3 r = quatLog(qErr);
        float rMat[3] = {r.x, r.y, r.z};

        // H = [I₃ 0]: mede os 3 componentes de atitude do estado de erro.
        // S = H P Hᵀ + R (3x3), K = P Hᵀ S⁻¹ (6x3)
        float S[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                S[i][j] = P_[i][j] + ((i == j) ? cfg_.measNoiseRad : 0.0f);

        // inversão 3x3 por adjunta
        float det = S[0][0] * (S[1][1] * S[2][2] - S[1][2] * S[2][1]) -
                    S[0][1] * (S[1][0] * S[2][2] - S[1][2] * S[2][0]) +
                    S[0][2] * (S[1][0] * S[2][1] - S[1][1] * S[2][0]);
        if (std::fabs(det) < 1e-12f) return;
        float invDet = 1.0f / det;
        float Sinv[3][3];
        Sinv[0][0] =  (S[1][1] * S[2][2] - S[1][2] * S[2][1]) * invDet;
        Sinv[0][1] = -(S[0][1] * S[2][2] - S[0][2] * S[2][1]) * invDet;
        Sinv[0][2] =  (S[0][1] * S[1][2] - S[0][2] * S[1][1]) * invDet;
        Sinv[1][0] = -(S[1][0] * S[2][2] - S[1][2] * S[2][0]) * invDet;
        Sinv[1][1] =  (S[0][0] * S[2][2] - S[0][2] * S[2][0]) * invDet;
        Sinv[1][2] = -(S[0][0] * S[1][2] - S[0][2] * S[1][0]) * invDet;
        Sinv[2][0] =  (S[1][0] * S[2][1] - S[1][1] * S[2][0]) * invDet;
        Sinv[2][1] = -(S[0][0] * S[2][1] - S[0][1] * S[2][0]) * invDet;
        Sinv[2][2] =  (S[0][0] * S[1][1] - S[0][1] * S[1][0]) * invDet;

        // K = P[:, 0:3] * Sinv (6x3)
        float K[6][3];
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 3; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 3; ++k) sum += P_[i][k] * Sinv[k][j];
                K[i][j] = sum;
            }

        // aplica correção: δatt = K_att·r ; δbias = K_bias·r
        Vec3 corr{0, 0, 0}, corrBias{0, 0, 0};
        const float kAtt[3][3] = {{K[0][0], K[0][1], K[0][2]},
                                  {K[1][0], K[1][1], K[1][2]},
                                  {K[2][0], K[2][1], K[2][2]}};
        corr.x = kAtt[0][0] * rMat[0] + kAtt[0][1] * rMat[1] + kAtt[0][2] * rMat[2];
        corr.y = kAtt[1][0] * rMat[0] + kAtt[1][1] * rMat[1] + kAtt[1][2] * rMat[2];
        corr.z = kAtt[2][0] * rMat[0] + kAtt[2][1] * rMat[1] + kAtt[2][2] * rMat[2];
        corrBias.x = K[3][0] * rMat[0] + K[3][1] * rMat[1] + K[3][2] * rMat[2];
        corrBias.y = K[4][0] * rMat[0] + K[4][1] * rMat[1] + K[4][2] * rMat[2];
        corrBias.z = K[5][0] * rMat[0] + K[5][1] * rMat[1] + K[5][2] * rMat[2];
        q_ = (quatExp(corr) * q_).normalized();
        bias_ = bias_ - corrBias; // bias positivo reduz ω na integração

        // P = (I - K H) P  com  H P = P[0:3][:]
        float HP[3][6];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 6; ++j) HP[i][j] = P_[i][j];
        for (int i = 0; i < 6; ++i)
            for (int j = 0; j < 6; ++j) {
                float delta = 0.0f;
                for (int k = 0; k < 3; ++k) delta += K[i][k] * HP[k][j];
                P_[i][j] -= delta;
            }

        // re-simetriza
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j) {
                float s = 0.5f * (P_[i][j] + P_[j][i]);
                P_[i][j] = s; P_[j][i] = s;
            }
    }

    // Predição rotacional à frente (reprojection de atitude) por tPred segundos.
    Quat predictAhead(const Vec3& gyroRadSec, float tPred) const {
        Vec3 w = gyroRadSec - bias_;
        float angle = w.length() * tPred;
        if (angle < kEps) return q_;
        return (Quat::fromAxisAngle(w, angle) * q_).normalized();
    }

    Quat orientation() const { return q_; }
    Vec3  gyroBias() const { return bias_; }

private:
    Config cfg_;
    Quat q_ = Quat::identity();
    Vec3 bias_{0, 0, 0};
    float P_[6][6]{};
};

} // namespace brazilmr
