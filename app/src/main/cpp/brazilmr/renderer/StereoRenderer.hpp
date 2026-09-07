// BrazilMR — renderização estéreo.
// Alvo por olho (FBO RGBA+depth) → Compositor faz o warp para a tela.
// O tamanho dos alvos segue renderScale e o retângulo de tela do olho.
#pragma once

#include "../gl/GlUtils.hpp"
#include "Camera.hpp"
#include "StereoConfig.hpp"
#include <array>

namespace brazilmr {

class StereoRenderer {
public:
    bool init();
    void destroy();

    // Recalcula alvos quando profile/tela mudam.
    void configure(const StereoProfile& profile, int screenW, int screenH);

    // Render de um olho. eye: 0=esquerdo, 1=direito (já considera swapEyes
    // na montagem da câmera? NÃO — o swap é de TELA/lente; as câmeras seguem
    // a anatomia: olho esquerdo sempre offset -IPD/2).
    // renderFn recebe a câmera configurada e desenha a cena.
    template <typename F>
    void renderEye(int eye, const Vec3& headPos, const Quat& headOrient,
                   F&& renderFn) {
        Framebuffer& fbo = eyeFbo_[eye];
        fbo.bind();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Camera& cam = eyeCam_[eye];
        float aspect = static_cast<float>(fbo.width()) /
                       static_cast<float>(fbo.height());
        float eyeOffset = (eye == 0 ? -1.0f : 1.0f) * profile_.ipdMeters() * 0.5f;
        cam.setupEye(headPos, headOrient, eyeOffset, profile_.eyeFovYRad(),
                     aspect, 0.02f, 60.0f);
        cam.extractFrustum();
        renderFn(cam);
        Framebuffer::bindDefault(screenW_, screenH_);
    }

    const Camera& eyeCamera(int eye) const { return eyeCam_[eye]; }
    Framebuffer& eyeFbo(int eye) { return eyeFbo_[eye]; }
    const StereoProfile& profile() const { return profile_; }
    int eyeTargetWidth(int eye) const { return eyeFbo_[eye].width(); }
    int eyeTargetHeight(int eye) const { return eyeFbo_[eye].height(); }

private:
    std::array<Framebuffer, 2> eyeFbo_;
    std::array<Camera, 2> eyeCam_;
    StereoProfile profile_;
    int screenW_ = 0, screenH_ = 0;
};

} // namespace brazilmr
