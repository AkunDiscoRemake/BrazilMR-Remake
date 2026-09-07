#include "SpatialWindow.hpp"
#include <cmath>
#include <algorithm>

namespace brazilmr {

void layoutArc(std::vector<SpatialWindow>& windows, float radiusM,
               float centerAngleDeg, float spreadDeg) {
    if (windows.empty()) return;
    float center = degToRad(centerAngleDeg);
    float spread = degToRad(spreadDeg);
    int n = static_cast<int>(windows.size());
    for (int i = 0; i < n; ++i) {
        float t = n == 1 ? 0.0f
                         : static_cast<float>(i) / static_cast<float>(n - 1) - 0.5f;
        float ang = center + t * spread;
        SpatialWindow& w = windows[i];
        w.position = {std::sin(ang) * radiusM, 0.0f, -std::cos(ang) * radiusM};
        // olha para o usuário (yaw = ang)
        w.orientation = Quat::fromEulerYXZ(ang, 0.0f, 0.0f);
    }
}

} // namespace brazilmr
