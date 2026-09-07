// BrazilMR — pipeline de filtragem de pose.
// RAW  →  ONE EURO  →  KALMAN  →  PREDICTION  →  FINAL POSE
// Cada estágio é ligável/desligável e parametrizável em runtime.
#pragma once

#include "../math/MathTypes.hpp"
#include "OneEuroFilter.hpp"
#include "KalmanFilter.hpp"

namespace brazilmr {

// Pose completa da cabeça (ou de um controller) no espaço do mundo.
struct Pose {
    Vec3  position{0, 0, 0};   // metros
    Quat  orientation = Quat::identity();
    Vec3  velocity{0, 0, 0};   // m/s (do Kalman)
    Vec3  angularVelocity{0, 0, 0}; // rad/s (para predição)
    int64_t timestampNs = 0;
    float  confidence = 0.0f;  // 0..1
    int    dofs = 3;           // graus de liberdade da fonte
};

struct PoseFilterConfig {
    // Estágio One Euro
    bool  useOneEuro     = true;
    OneEuroConfig oneEuro;
    // Estágio Kalman (posição)
    bool  useKalmanPos   = true;
    KalmanPosVel::Config kalmanPos;
    // Estágio Kalman (orientação por espaço de erro)
    bool  useOrientationEkf = true;
    OrientationEkf::Config orientationEkf;
    // Predição à frente (anti-latência; ~meio frame a um frame).
    bool  usePrediction  = true;
    float predictionTimeSec = 0.012f;
    // Ganho de confiança aplicado quando o tracking cai.
    bool  enableConfidenceDamping = true;
};

class PoseFilterPipeline {
public:
    explicit PoseFilterPipeline(const PoseFilterConfig& cfg = PoseFilterConfig())
        : cfg_(cfg) {}

    void configure(const PoseFilterConfig& cfg) {
        cfg_ = cfg;
        posOneEuro_.configure(cfg.oneEuro);
        quatOneEuro_.configure(cfg.oneEuro);
        reset();
    }
    const PoseFilterConfig& config() const { return cfg_; }

    void reset() {
        posOneEuro_.reset();
        quatOneEuro_.reset();
        posKalman_.reset(Vec3{0, 0, 0});
        orientEkf_.reset();
        hasPrev_ = false;
    }

    // Alimenta o pipeline com uma pose CRUA do backend de tracking.
    Pose process(const Pose& raw) {
        Pose out = raw;
        float dt = hasPrev_
                       ? nsToSecondsF(raw.timestampNs - prevTimestampNs_)
                       : 1.0f / 60.0f;
        if (dt <= 0.0f || dt > 0.5f) dt = 1.0f / 60.0f;
        prevTimestampNs_ = raw.timestampNs;
        hasPrev_ = true;

        // 1) One Euro — suaviza jitter mantendo responsividade.
        if (cfg_.useOneEuro) {
            out.position = posOneEuro_.filter(raw.position, dt);
            out.orientation = quatOneEuro_.filter(raw.orientation, dt);
        }

        // 2) Kalman — estimativa de estado + rejeição de ruído.
        if (cfg_.useKalmanPos) {
            posKalman_.predict(dt);
            posKalman_.update(out.position);
            out.position = posKalman_.position();
            out.velocity = posKalman_.velocity();
        }
        if (cfg_.useOrientationEkf) {
            orientEkf_.integrateGyro(raw.angularVelocity, dt);
            orientEkf_.updateAttitude(out.orientation);
            out.orientation = orientEkf_.orientation();
        }

        // 3) Predição — compensa latência sensor→fóton.
        if (cfg_.usePrediction) {
            float t = cfg_.predictionTimeSec;
            if (cfg_.useKalmanPos) out.position = posKalman_.predictAhead(t);
            if (cfg_.useOrientationEkf)
                out.orientation = orientEkf_.predictAhead(raw.angularVelocity, t);
        }

        if (cfg_.enableConfidenceDamping && raw.confidence < 0.5f) {
            // Tracking fraco: mistura suave com a última pose boa.
            float k = clampf(raw.confidence * 2.0f, 0.05f, 1.0f);
            out.position = Vec3::lerp(lastGood_.position, out.position, k);
            out.orientation = Quat::slerp(lastGood_.orientation, out.orientation, k);
        }
        if (raw.confidence > 0.5f) lastGood_ = out;
        return out;
    }

    const KalmanPos3& positionKalman() const { return posKalman_; }
    const OrientationEkf& orientationEkf() const { return orientEkf_; }

private:
    static float nsToSecondsF(int64_t ns) { return static_cast<float>(ns) * 1e-9f; }

    PoseFilterConfig cfg_;
    Vec3OneEuro  posOneEuro_;
    QuatOneEuro  quatOneEuro_;
    KalmanPos3   posKalman_;
    OrientationEkf orientEkf_;
    Pose  lastGood_;
    int64_t prevTimestampNs_ = 0;
    bool  hasPrev_ = false;
};

} // namespace brazilmr
