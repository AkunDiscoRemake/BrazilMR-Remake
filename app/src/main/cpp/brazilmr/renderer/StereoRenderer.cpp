#include "StereoRenderer.hpp"

namespace brazilmr {

bool StereoRenderer::init() {
    return true;
}

void StereoRenderer::destroy() {
    eyeFbo_[0].destroy();
    eyeFbo_[1].destroy();
    screenW_ = screenH_ = 0;
}

void StereoRenderer::configure(const StereoProfile& profile, int screenW,
                               int screenH) {
    profile_ = profile;
    screenW_ = screenW;
    screenH_ = screenH;

    Rect left, right;
    computeEyeRects(screenW, screenH, left, right);
    Rect rects[2] = {left, right};

    for (int eye = 0; eye < 2; ++eye) {
        // escala interna do alvo (renderScale) — o warp amostra suavemente
        int w = static_cast<int>(rects[eye].w * profile_.renderScale);
        int h = static_cast<int>(rects[eye].h * profile_.renderScale);
        if (w < 32) w = 32;
        if (h < 32) h = 32;
        if (!eyeFbo_[eye].valid() || eyeFbo_[eye].width() != w ||
            eyeFbo_[eye].height() != h) {
            eyeFbo_[eye].create(w, h, true);
        }
    }
}

} // namespace brazilmr
