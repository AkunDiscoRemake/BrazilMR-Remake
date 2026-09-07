// BrazilMR — tipos comuns de tracking.
#pragma once

#include <cstdint>

namespace brazilmr {

enum class TrackingBackend : int {
    NONE    = 0,  // sem sensores
    IMU_3DOF = 1, // giroscópio + acelerômetro (+ magnetômetro)
    CV_SLAM = 2,  // odometria visual própria (Camera2 + CV)
    ARCORE  = 3,  // ARCore world tracking (6DoF)
};

inline const char* trackingBackendName(TrackingBackend b) {
    switch (b) {
        case TrackingBackend::IMU_3DOF: return "IMU_3DOF";
        case TrackingBackend::CV_SLAM:  return "CV_SLAM";
        case TrackingBackend::ARCORE:   return "ARCORE";
        default: return "NONE";
    }
}

// Capacidades reportadas pelo dispositivo (usado pelos fallbacks automáticos).
struct TrackingCapabilities {
    bool hasGyroscope     = false;
    bool hasAccelerometer = false;
    bool hasMagnetometer  = false;
    bool hasRearCamera    = false;
    bool arcoreAvailable  = false;
    bool handTrackingOk   = false;
};

} // namespace brazilmr
