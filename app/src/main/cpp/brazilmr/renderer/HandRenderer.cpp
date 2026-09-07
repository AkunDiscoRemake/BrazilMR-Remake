#include "HandRenderer.hpp"
#include <cmath>

namespace brazilmr {

// Conexões ósseas (índices de landmarks) + espessura relativa.
struct BoneDef { int a, b; float width; };
static const BoneDef kBones[] = {
    // polegar
    {0, 1, 0.013f}, {1, 2, 0.012f}, {2, 3, 0.011f}, {3, 4, 0.010f},
    // indicador
    {0, 5, 0.013f}, {5, 6, 0.011f}, {6, 7, 0.010f}, {7, 8, 0.009f},
    // médio
    {9, 10, 0.011f}, {10, 11, 0.010f}, {11, 12, 0.009f},
    // anelar
    {13, 14, 0.010f}, {14, 15, 0.0095f}, {15, 16, 0.009f},
    // mínimo
    {17, 18, 0.0095f}, {18, 19, 0.009f}, {19, 20, 0.0085f},
    // palma
    {0, 9, 0.014f}, {0, 13, 0.013f}, {0, 17, 0.012f}, {5, 9, 0.012f},
    {9, 13, 0.011f}, {13, 17, 0.010f},
};
constexpr int kBoneCount = static_cast<int>(sizeof(kBones) / sizeof(kBones[0]));
constexpr int kMaxBoneInstances = 64; // 2 mãos × ~24 ossos

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------
static const char* kBoneVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec4 aInst0;
layout(location = 3) in vec4 aInst1;
layout(location = 4) in vec4 aInst2;
layout(location = 5) in vec4 aInst3;
layout(location = 6) in vec4 aParams;  // rgb=cor, a=glow

uniform mat4 uViewProj;
out vec3 vWorld;
out vec3 vNormal;
out vec3 vColor;
out float vGlow;

void main() {
    mat4 model = mat4(aInst0, aInst1, aInst2, aInst3);
    vec4 world = model * vec4(aPos, 1.0);
    vWorld = world.xyz;
    vNormal = mat3(model) * aNrm;
    vColor = aParams.rgb;
    vGlow = aParams.a;
    gl_Position = uViewProj * world;
}
)";

static const char* kBoneFs = R"(#version 300 es
precision highp float;
in vec3 vWorld;
in vec3 vNormal;
in vec3 vColor;
in float vGlow;
uniform vec3 uCamPos;
uniform vec3 uLightDir;
out vec4 outColor;
void main() {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorld);
    vec3 L = normalize(-uLightDir);
    float ndl = max(dot(N, L), 0.0);
    float rim = pow(1.0 - max(dot(N, V), 0.0), 2.5);
    vec3 col = vColor * (0.35 + 0.65 * ndl);
    col += vColor * rim * 0.8;              // contorno suave
    col += vec3(0.0, 0.9, 0.63) * vGlow;    // highlight (pinch/point)
    outColor = vec4(col, 1.0);
}
)";

static const char* kFxVs = R"(#version 300 es
precision highp float;
layout(location = 0) in vec2 aQuad;   // -1..1 billboard
layout(location = 1) in vec4 aInstPosSize; // xyz, size
layout(location = 2) in vec4 aInstColor;   // rgb, alpha
uniform mat4 uViewProj;
uniform vec3 uRight;
uniform vec3 uUp;
out vec2 vUv;
out vec4 vColor;
void main() {
    vUv = aQuad;
    vColor = aInstColor;
    vec3 world = aInstPosSize.xyz + uRight * (aQuad.x * aInstPosSize.w)
                                    + uUp * (aQuad.y * aInstPosSize.w);
    gl_Position = uViewProj * vec4(world, 1.0);
}
)";

static const char* kFxFs = R"(#version 300 es
precision highp float;
in vec2 vUv;
in vec4 vColor;
uniform int uMode; // 0 = partícula (círculo suave), 1 = anel
out vec4 outColor;
void main() {
    float r = length(vUv);
    float a;
    if (uMode == 1) {
        float ring = 1.0 - smoothstep(0.02, 0.0, abs(r - 0.82));
        a = ring;
    } else {
        a = smoothstep(1.0, 0.2, r);
    }
    outColor = vec4(vColor.rgb, vColor.a * a);
}
)";

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
bool HandRenderer::init() {
    if (!boneShader_.build(kBoneVs, kBoneFs)) return false;
    if (!fxShader_.build(kFxVs, kFxFs)) return false;

    buildCapsule();

    // quad billboard (2 triângulos)
    static const float quad[] = {
        -1, -1, 1, -1, 1, 1,   -1, -1, 1, 1, -1, 1
    };
    std::vector<VertexAttrib> qAttrs = {{2, GL_FLOAT, false}};
    if (!quadMesh_.upload(quad, 6, 2 * sizeof(float), qAttrs, nullptr, 0,
                          GL_UNSIGNED_SHORT))
        return false;

    ready_ = true;
    return true;
}

void HandRenderer::destroy() {
    boneShader_.destroy();
    fxShader_.destroy();
    capsuleMesh_.destroy();
    quadMesh_.destroy();
    ready_ = false;
}

void HandRenderer::buildCapsule() {
    // cápsula unitária: raio 1 em XZ, de y=0 a y=1, com hemisférios.
    const int seg = 10;   // ao redor
    const int rings = 6;  // por hemisfério
    std::vector<float> verts;      // pos3 + nrm3
    std::vector<uint16_t> idx;

    auto pushVert = [&](float x, float y, float z) {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len < 1e-6f) len = 1.0f;
        verts.push_back(x); verts.push_back(y); verts.push_back(z);
        verts.push_back(x / len); verts.push_back(y / len); verts.push_back(z / len);
    };

    // topo (hemisfério em y=1)
    for (int r = 0; r <= rings; ++r) {
        float phi = (static_cast<float>(r) / rings) * (kPiHalf);
        for (int s = 0; s <= seg; ++s) {
            float th = (static_cast<float>(s) / seg) * (2.0f * kPi);
            float cphi = std::cos(phi), sphi = std::sin(phi);
            pushVert(std::cos(th) * cphi, 1.0f + sphi, std::sin(th) * cphi);
        }
    }
    // base (hemisfério em y=0)
    for (int r = 0; r <= rings; ++r) {
        float phi = (static_cast<float>(r) / rings) * (kPiHalf);
        for (int s = 0; s <= seg; ++s) {
            float th = (static_cast<float>(s) / seg) * (2.0f * kPi);
            float cphi = std::cos(phi), sphi = std::sin(phi);
            pushVert(std::cos(th) * cphi, -sphi, std::sin(th) * cphi);
        }
    }
    int perRing = seg + 1;
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < seg; ++s) {
            uint16_t a = static_cast<uint16_t>(r * perRing + s);
            uint16_t b = a + 1;
            uint16_t c = a + perRing;
            uint16_t d = c + 1;
            idx.insert(idx.end(), {a, c, b, b, c, d});
            uint16_t a2 = static_cast<uint16_t>((rings + 1 + r) * perRing + s);
            uint16_t b2 = a2 + 1;
            uint16_t c2 = a2 + perRing;
            uint16_t d2 = c2 + 1;
            idx.insert(idx.end(), {a2, c2, b2, b2, c2, d2});
        }

    std::vector<VertexAttrib> attrs = {{3, GL_FLOAT, false}, {3, GL_FLOAT, false}};
    capsuleMesh_.upload(verts.data(), verts.size() / 6, 6 * sizeof(float), attrs,
                        idx.data(), idx.size(), GL_UNSIGNED_SHORT);
}

void HandRenderer::update(const HandState& left, const HandState& right,
                          float dtSec, float timeSec) {
    hands_[0] = left;
    hands_[1] = right;
    time_ = timeSec;

    // partículas: spawn nas pontas dos dedos durante pinch/movimento rápido
    for (int h = 0; h < 2; ++h) {
        const HandState& hand = hands_[h];
        if (!hand.present) continue;
        float speed = hand.velocity.length();
        bool emit = hand.gesture == HandGesture::PINCH || speed > 1.5f;
        if (!emit) continue;
        float rate = clampf(speed * 4.0f + hand.pinchStrength * 12.0f, 0.0f, 20.0f);
        int spawn = static_cast<int>(rate * dtSec);
        for (int s = 0; s < spawn && s < 2; ++s) {
            Particle& p = particles_[particleHead_];
            particleHead_ = (particleHead_ + 1) % limits::kMaxHandParticles;
            p.pos = hand.indexTip;
            p.vel = Vec3{
                (std::sin(time_ * 31.0f + s * 2.1f)) * 0.25f,
                0.15f + 0.1f * std::cos(time_ * 17.0f + s),
                (std::cos(time_ * 23.0f + s * 1.7f)) * 0.25f};
            p.maxLife = 0.35f;
            p.life = p.maxLife;
            p.size = 0.006f + 0.004f * hand.pinchStrength;
        }
    }
    // envelhecimento
    for (auto& p : particles_)
        if (p.life > 0.0f) {
            p.life -= dtSec;
            p.pos = p.pos + p.vel * dtSec;
            p.vel.y -= 0.6f * dtSec;
        }

    int i = 0;
    while (i < rippleCount_) {
        ripples_[i].age += dtSec;
        if (ripples_[i].age >= ripples_[i].maxAge) {
            ripples_[i] = ripples_[rippleCount_ - 1];
            --rippleCount_;
            continue; // reprocessa o slot substituído
        }
        ++i;
    }
}

void HandRenderer::spawnRipple(const Vec3& point, const Vec3& normal) {
    if (rippleCount_ >= limits::kMaxRipples) {
        ripples_[0] = ripples_[rippleCount_ - 1];
        --rippleCount_;
    }
    Ripple& r = ripples_[rippleCount_++];
    r.pos = point;
    r.normal = normal;
    r.age = 0.0f;
    r.maxAge = 0.5f;
}

// Monta matriz de cápsula de a→b (eixo Y da cápsula → direção a→b).
static Mat4 capsuleMatrix(const Vec3& a, const Vec3& b, float radius) {
    Vec3 dir = b - a;
    float len = dir.length();
    if (len < 1e-5f) return Mat4::translation(a);
    dir = dir / len;
    // quaternion que leva +Y para dir
    Vec3 up{0, 1, 0};
    Vec3 axis = up.cross(dir);
    float dot = up.dot(dir);
    Quat rot;
    if (axis.lengthSq() < 1e-8f) {
        rot = dot > 0 ? Quat::identity()
                      : Quat::fromAxisAngle(Vec3{1, 0, 0}, kPi);
    } else {
        float angle = std::acos(clampf(dot, -1.0f, 1.0f));
        rot = Quat::fromAxisAngle(axis, angle);
    }
    return Mat4::translation(a) * Mat4::rotation(rot) *
           Mat4::scale(Vec3{radius, len, radius});
}

void HandRenderer::draw(const Camera& cam, const EffectQuality& fx) {
    if (!ready_) return;

    // --- ossos das mãos (desenho direto, uma cápsula por osso) ---
    boneShader_.use();
    // NOTA: o shader usa atributos de instância (locations 2..6). Sem VBO de
    // instância dedicado, alimentamos os atributos com glVertexAttrib* antes
    // de cada draw — zero uploads, GPU resolve o resto.
    for (int h = 0; h < 2; ++h) {
        const HandState& hand = hands_[h];
        if (!hand.present) continue;

        float pinchGlow = hand.pinchStrength * 1.2f * fx.handGlow;
        bool pointing = hand.gesture == HandGesture::POINT;
        bool grabbing = hand.gesture == HandGesture::GRAB;
        const Vec3 grey{0.62f, 0.66f, 0.71f};
        const Vec3 accent{0.0f, 0.9f, 0.63f};

        boneShader_.setMat4("uViewProj", cam.viewProj);
        boneShader_.setVec3("uCamPos", cam.position);
        boneShader_.setVec3("uLightDir", Vec3{0.4f, -0.8f, 0.3f});

        for (int i = 0; i < kBoneCount; ++i) {
            const BoneDef& bd = kBones[i];
            Mat4 m = capsuleMatrix(hand.landmarks[bd.a], hand.landmarks[bd.b],
                                   bd.width * (grabbing ? 0.85f : 1.0f));
            Vec3 col = grey;
            float glow = 0.0f;
            bool pinchBone = (bd.a >= 1 && bd.a <= 8) || (bd.b >= 1 && bd.b <= 8);
            if (pinchBone && pinchGlow > 0.01f) {
                col = Vec3::lerp(grey, accent, clampf(pinchGlow, 0.0f, 1.0f) * 0.55f);
                glow = pinchGlow * 0.45f;
            }
            if (pointing && bd.a >= 5 && bd.b <= 8) {
                col = Vec3::lerp(grey, accent, 0.65f);
                glow = 0.85f;
            }
            if (grabbing) glow = std::fmax(glow, 0.30f);

            // alimenta atributos de "instância" como constantes de vértice
            glVertexAttrib4f(2, m.m[0], m.m[1], m.m[2], m.m[3]);
            glVertexAttrib4f(3, m.m[4], m.m[5], m.m[6], m.m[7]);
            glVertexAttrib4f(4, m.m[8], m.m[9], m.m[10], m.m[11]);
            glVertexAttrib4f(5, m.m[12], m.m[13], m.m[14], m.m[15]);
            glVertexAttrib4f(6, col.x, col.y, col.z, glow);
            capsuleMesh_.draw();
        }
    }

    // --- FX: anel de pinça + partículas + ripples ---
    fxShader_.use();
    fxShader_.setMat4("uViewProj", cam.viewProj);
    Vec3 camRight = cam.orientation.rotate(Vec3{1, 0, 0});
    Vec3 camUp = cam.orientation.rotate(Vec3{0, 1, 0});
    fxShader_.setVec3("uRight", camRight);
    fxShader_.setVec3("uUp", camUp);
    fxShader_.setInt("uMode", 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const Vec3 accent{0.0f, 0.9f, 0.63f};

    // partículas
    for (const auto& p : particles_) {
        if (p.life <= 0.0f) continue;
        float t = p.life / p.maxLife;
        glVertexAttrib4f(1, p.pos.x, p.pos.y, p.pos.z, p.size * (0.5f + t * 0.5f));
        glVertexAttrib4f(2, accent.x, accent.y, accent.z, 0.55f * t * fx.handParticles);
        quadMesh_.draw();
    }

    // anéis: modo 1 (ripple no mundo + anel de pinça na mão)
    fxShader_.setInt("uMode", 1);
    for (int h = 0; h < 2; ++h) {
        const HandState& hand = hands_[h];
        if (!hand.present || hand.pinchStrength < 0.05f) continue;
        float s = 0.018f + 0.012f * (1.0f - hand.pinchStrength);
        glVertexAttrib4f(1, hand.pinchPoint.x, hand.pinchPoint.y, hand.pinchPoint.z, s);
        glVertexAttrib4f(2, accent.x, accent.y, accent.z,
                         hand.pinchStrength * 0.9f * fx.handGlow);
        quadMesh_.draw();
    }
    for (int i = 0; i < rippleCount_; ++i) {
        const Ripple& r = ripples_[i];
        float t = r.age / r.maxAge;
        float size = 0.03f + t * 0.16f;
        glVertexAttrib4f(1, r.pos.x, r.pos.y, r.pos.z, size);
        glVertexAttrib4f(2, accent.x, accent.y, accent.z, (1.0f - t) * 0.8f);
        quadMesh_.draw();
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    // limpa constantes de atributo para não vazar para outros draws
    for (int loc = 1; loc <= 6; ++loc)
        glDisableVertexAttribArray(0), glVertexAttrib4f(loc, 0, 0, 0, 0);
    (void)0;
}

} // namespace brazilmr
