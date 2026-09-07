// BrazilMR — odometria visual monocular (backend CV de 6DoF).
//   CAMERA → FRAME PREPROC → FEATURE DETECTION → FEATURE TRACKING →
//   MOTION ESTIMATION (essencial + RANSAC) → VISUAL ODOMETRY → POSE
// A escala métrica vem do acoplamento frouxo com o IMU (acelerômetro),
// quando disponível. É o backend de fallback quando ARCore não existe.
#pragma once

#include "../math/MathTypes.hpp"
#include "OpticalFlow.hpp"
#include <cstdint>
#include <vector>

namespace brazilmr {

struct VoResult {
    bool  ok = false;
    Quat  rotation = Quat::identity();  // delta de rotação desde o último frame
    Vec3  translationUnit{0, 0, 0};     // direção de translação (escala 1)
    int   trackedFeatures = 0;
    int   inliers = 0;
    float reprojectionError = 0.0f;
};

struct VoConfig {
    int   fastThreshold      = 22;    // sensibilidade do detector
    int   maxFeatures        = 140;   // orçamento de features
    int   minTracksForPose   = 24;    // abaixo disso, só rotação por IMU
    float ransacThreshold    = 1.5e-3f; // erro epipolar normalizado
    int   ransacIterations   = 120;
    int   pyramidLevels      = 3;
    int   minInlierRatio     = 40;    // % mínimo de inliers
    // Acoplamento de escala IMU↔VO
    bool  imuScaleCoupling   = true;
    float minImuSpeedMps     = 0.15f; // abaixo disso a escala não é estimada
    float maxScale           = 2.5f;
    float scaleEma           = 0.05f;
};

class VisualOdometry {
public:
    explicit VisualOdometry(const VoConfig& cfg = VoConfig()) : cfg_(cfg) {}

    void configure(const VoConfig& cfg) { cfg_ = cfg; }
    const VoConfig& config() const { return cfg_; }

    void reset() {
        hasPrev_ = false;
        voPosition_ = {0, 0, 0};
        voRotation_ = Quat::identity();
        scale_ = 1.0f;
        tracks_.clear();
        imuDispWindow_.clear();
        voDispWindow_.clear();
    }

    // Processa um frame de luminância. Chame a 15–30 Hz (presets de câmera).
    VoResult processFrame(const LumaView& luma, int64_t timestampNs);

    // Alimenta o deslocamento do IMU (m/s² integrado em metros, desde a última
    // chamada) para estimar a escala métrica da translação monoculada.
    void feedImuDisplacement(const Vec3& displacementMeters);

    // Pose acumulada (escala métrica estimada).
    Vec3 position() const { return voPosition_; }
    Quat rotation() const { return voRotation_; }
    float scale() const { return scale_; }
    int  lastFeatureCount() const { return lastFeatures_; }

private:
    // Estima R e t̂ (unitário) de dois conjuntos de pontos normalizados.
    bool estimateMotion(const std::vector<Vec2>& prev, const std::vector<Vec2>& cur,
                        Quat& rotOut, Vec3& transUnitOut, int& inliersOut,
                        float& errOut);

    VoConfig cfg_;
    ImagePyramid prevPyr_, curPyr_;
    std::vector<KltTrack> tracks_;
    bool hasPrev_ = false;
    int64_t lastTsNs_ = 0;

    Vec3  voPosition_{0, 0, 0};
    Quat  voRotation_ = Quat::identity();
    float scale_ = 1.0f;
    int   lastFeatures_ = 0;

    // janelas para casamento de escala IMU↔VO
    std::vector<Vec3> imuDispWindow_;
    std::vector<Vec3> voDispWindow_;
    Vec3 imuAccum_{0, 0, 0};
    Vec3 voAccum_{0, 0, 0};
};

} // namespace brazilmr
