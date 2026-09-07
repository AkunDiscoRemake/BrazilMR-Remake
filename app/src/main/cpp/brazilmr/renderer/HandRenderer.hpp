// BrazilMR — renderização das mãos VR.
// Mãos cinza estilizadas: cápsulas por osso, iluminação simples + rim light,
// anel de pinça, highlight de dedos, partículas leves e ripples de clique.
// Tudo instanciado: 1 draw call para os ossos, 1 para as pontas, 1 para FX.
#pragma once

#include "../BrazilmrConfig.hpp"
#include "../gl/GlUtils.hpp"
#include "../hands/HandTypes.hpp"
#include "Camera.hpp"

namespace brazilmr {

class HandRenderer {
public:
    bool init();
    void destroy();

    // Simula partículas/ripples e armazena pose das mãos para o frame.
    void update(const HandState& left, const HandState& right, float dtSec,
                float timeSec);

    void draw(const Camera& cam, const EffectQuality& fx);

    // Ripple espacial em um ponto do mundo (feedback de clique/trigger).
    void spawnRipple(const Vec3& point, const Vec3& normal);

private:
    struct Particle {
        Vec3 pos;
        Vec3 vel;
        float life = 0.0f;      // restante
        float maxLife = 0.4f;
        float size = 0.01f;
    };
    struct Ripple {
        Vec3 pos;
        Vec3 normal;
        float age = 0.0f;
        float maxAge = 0.5f;
    };

    void buildCapsule();

    Shader boneShader_;
    Shader fxShader_;
    Mesh capsuleMesh_;   // unit: raio 1, comprimento 1 (eixo Y 0..1)
    Mesh quadMesh_;      // billboard para partículas/anel
    bool ready_ = false;

    HandState hands_[2];
    float time_ = 0.0f;

    Particle particles_[limits::kMaxHandParticles];
    int particleHead_ = 0;
    Ripple ripples_[limits::kMaxRipples];
    int rippleCount_ = 0;
};

} // namespace brazilmr
