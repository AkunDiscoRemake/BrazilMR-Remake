#include "GltfRuntime.hpp"
#include <cstring>

namespace brazilmr {

using Node = RuntimeNode;

// ---------------------------------------------------------------------------
// Conversão glTF → runtime
// ---------------------------------------------------------------------------
std::shared_ptr<RuntimeModel> RuntimeModel::fromGltf(const GltfModel& g,
                                                     std::string& err) {
    auto model = std::make_shared<RuntimeModel>();

    // nós
    model->nodes_.resize(g.nodes.size());
    for (std::size_t i = 0; i < g.nodes.size(); ++i) {
        const GltfNode& gn = g.nodes[i];
        Node& rn = model->nodes_[i];
        rn.name = gn.name;
        rn.translation = {gn.translation[0], gn.translation[1], gn.translation[2]};
        rn.rotation = {gn.rotation[0], gn.rotation[1], gn.rotation[2], gn.rotation[3]};
        rn.scale = {gn.scale[0], gn.scale[1], gn.scale[2]};
        rn.children = gn.children;
        rn.skin = gn.skin;
        if (gn.hasMatrix) {
            // decomposição simplificada: usa colunas como base TRS
            Mat4 m;
            for (int k = 0; k < 16; ++k) m.m[k] = gn.matrix[k];
            rn.translation = m.translation();
            // escala = comprimento das colunas
            Vec3 c0{m.m[0], m.m[1], m.m[2]};
            Vec3 c1{m.m[4], m.m[5], m.m[6]};
            Vec3 c2{m.m[8], m.m[9], m.m[10]};
            float sx = c0.length(), sy = c1.length(), sz = c2.length();
            if (sx > 1e-8f && sy > 1e-8f && sz > 1e-8f) {
                rn.scale = {sx, sy, sz};
                Mat4 rotOnly;
                for (int c = 0; c < 3; ++c) {
                    rotOnly.m[c * 4 + 0] = m.m[c * 4 + 0] / sx;
                    rotOnly.m[c * 4 + 1] = m.m[c * 4 + 1] / sy;
                    rotOnly.m[c * 4 + 2] = m.m[c * 4 + 2] / sz;
                }
                rn.rotation = rotOnly.toQuat();
            }
        }
        if (gn.mesh >= 0 && gn.mesh < static_cast<int>(g.meshes.size())) {
            const GltfMesh& gm = g.meshes[gn.mesh];
            for (std::size_t p = 0; p < gm.primitives.size(); ++p) {
                RuntimeNodeMesh nm;
                nm.meshIndex = static_cast<int>(model->meshes_.size());
                model->meshes_.push_back(RuntimeMesh());
                RuntimeMesh& rm = model->meshes_.back();
                const GltfPrimitive& prim = gm.primitives[p];

                // atributos
                for (int a = 0; a < prim.attrCount; ++a) {
                    int code = prim.attributes[a][0];
                    int acc = prim.attributes[a][1];
                    switch (code) {
                        case 0: decodeAccessorFloat(g, acc, rm.positions); break;
                        case 1: decodeAccessorFloat(g, acc, rm.normals); break;
                        case 2: decodeAccessorFloat(g, acc, rm.uvs); break;
                        case 3: decodeAccessorFloat(g, acc, rm.joints); break;
                        case 4: decodeAccessorFloat(g, acc, rm.weights); break;
                        default: break;
                    }
                }
                if (prim.indices >= 0)
                    decodeAccessorUint(g, prim.indices, rm.indices);
                else {
                    // não-indexado
                    std::size_t vc = rm.positions.size() / 3;
                    rm.indices.resize(vc);
                    for (std::size_t k = 0; k < vc; ++k) rm.indices[k] = static_cast<uint32_t>(k);
                }
                rm.material = prim.material >= 0 ? prim.material : 0;
                rm.hasSkin = (gn.skin >= 0);

                // valida contagens
                std::size_t vc = rm.positions.size() / 3;
                if (rm.normals.size() / 3 != vc) rm.normals.clear();
                if (rm.uvs.size() / 2 != vc) rm.uvs.clear();
                if (rm.joints.size() / 4 != vc || rm.weights.size() / 4 != vc) {
                    rm.joints.clear();
                    rm.weights.clear();
                }
                if (vc == 0) { model->meshes_.pop_back(); continue; }

                // AABB
                rm.aabbMin = {1e9f, 1e9f, 1e9f};
                rm.aabbMax = {-1e9f, -1e9f, -1e9f};
                for (std::size_t v = 0; v < vc; ++v) {
                    float x = rm.positions[v * 3], y = rm.positions[v * 3 + 1],
                          z = rm.positions[v * 3 + 2];
                    if (x < rm.aabbMin.x) rm.aabbMin.x = x;
                    if (y < rm.aabbMin.y) rm.aabbMin.y = y;
                    if (z < rm.aabbMin.z) rm.aabbMin.z = z;
                    if (x > rm.aabbMax.x) rm.aabbMax.x = x;
                    if (y > rm.aabbMax.y) rm.aabbMax.y = y;
                    if (z > rm.aabbMax.z) rm.aabbMax.z = z;
                }
                rn.meshes.push_back(nm);
            }
        }
    }

    // pais
    for (std::size_t i = 0; i < model->nodes_.size(); ++i)
        for (int c : model->nodes_[i].children)
            if (c >= 0 && c < static_cast<int>(model->nodes_.size()))
                model->nodes_[c].parent = static_cast<int>(i);

    // raízes
    model->rootNodes_ = g.sceneNodes;
    for (std::size_t i = 0; i < model->nodes_.size(); ++i)
        if (model->nodes_[i].parent < 0) {
            bool already = false;
            for (int r : model->rootNodes_) if (r == static_cast<int>(i)) already = true;
            if (!already) model->rootNodes_.push_back(static_cast<int>(i));
        }

    // materiais
    for (auto& gm : g.materials) {
        RuntimeMaterial rm;
        for (int k = 0; k < 4; ++k) rm.baseColorFactor[k] = gm.baseColorFactor[k];
        rm.metallicFactor = gm.metallicFactor;
        rm.roughnessFactor = gm.roughnessFactor;
        for (int k = 0; k < 3; ++k) rm.emissiveFactor[k] = gm.emissiveFactor[k];
        rm.alphaMode = gm.alphaMode;
        rm.alphaCutoff = gm.alphaCutoff;
        rm.doubleSided = gm.doubleSided;
        if (gm.baseColorTexture >= 0 &&
            gm.baseColorTexture < static_cast<int>(g.textures.size())) {
            int src = g.textures[gm.baseColorTexture].source;
            if (src >= 0 && src < static_cast<int>(g.images.size()))
                rm.baseColorImage = src;
        }
        model->materials_.push_back(rm);
    }

    // imagens (blobs — decodificação PNG/JPEG na camada superior)
    for (auto& gi : g.images) {
        RuntimeImage ri;
        ri.name = gi.name;
        ri.mimeType = gi.mimeType;
        if (gi.bufferView >= 0) {
            BufferSpan span = bufferViewSpan(g, gi.bufferView);
            if (span.data) ri.bytes.assign(span.data, span.data + span.size);
        } else if (gi.uri.rfind("data:", 0) == 0) {
            // data URI base64
            std::size_t comma = gi.uri.find(',');
            if (comma != std::string::npos) {
                const std::string& b64 = gi.uri.substr(comma + 1);
                static const char tbl[] =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                auto b64val = [&](char ch) -> int {
                    const char* p = strchr(tbl, ch);
                    return p ? static_cast<int>(p - tbl) : -1;
                };
                int acc = 0, bits = 0;
                ri.bytes.reserve(b64.size() * 3 / 4 + 3);
                for (char ch : b64) {
                    if (ch == '=' || ch == '\n' || ch == '\r') continue;
                    int v = b64val(ch);
                    if (v < 0) continue;
                    acc = (acc << 6) | v;
                    bits += 6;
                    if (bits >= 8) {
                        bits -= 8;
                        ri.bytes.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF));
                    }
                }
            }
        }
        model->images_.push_back(std::move(ri));
    }

    // animações
    for (auto& ga : g.animations) {
        RuntimeAnimation ra;
        ra.name = ga.name;
        ra.duration = 0.0f;
        for (auto& gs : ga.samplers) {
            RuntimeAnimSampler rs;
            rs.comps = 1;
            // comps do canal = componentes do ACESSOR DE SAÍDA (VEC3/VEC4/SCALAR)
            if (gs.output >= 0 && gs.output < static_cast<int>(g.accessors.size()))
                rs.comps = g.accessors[gs.output].numComponents();
            if (rs.comps <= 0) rs.comps = 1;
            decodeAccessorFloat(g, gs.input, rs.times);
            decodeAccessorFloat(g, gs.output, rs.values);
            rs.interpolation = gs.interpolation;
            if (!rs.times.empty())
                ra.duration = std::fmax(ra.duration, rs.times.back());
            ra.samplers.push_back(std::move(rs));
        }
        for (auto& gc : ga.channels) {
            RuntimeAnimChannel rc;
            rc.node = gc.targetNode;
            rc.path = gc.path;
            rc.sampler = gc.sampler;
            ra.channels.push_back(rc);
        }
        model->animations_.push_back(std::move(ra));
    }

    // skins
    for (auto& gs : g.skins) {
        RuntimeSkin rs;
        rs.jointNodes = gs.joints;
        rs.inverseBindMatrices.resize(gs.joints.size());
        if (gs.inverseBindMatrices >= 0) {
            std::vector<float> mats;
            if (decodeAccessorFloat(g, gs.inverseBindMatrices, mats) &&
                mats.size() >= gs.joints.size() * 16) {
                for (std::size_t j = 0; j < gs.joints.size(); ++j)
                    for (int k = 0; k < 16; ++k)
                        rs.inverseBindMatrices[j].m[k] = mats[j * 16 + k];
            } else {
                for (auto& m : rs.inverseBindMatrices) m = Mat4::identity();
            }
        }
        rs.jointMatrices.resize(gs.joints.size());
        for (std::size_t j = 0; j < rs.jointNodes.size(); ++j)
            rs.jointMatrices[j] = Mat4::identity();
        model->skins_.push_back(std::move(rs));
    }

    // AABB global
    model->aabbMin_ = {1e9f, 1e9f, 1e9f};
    model->aabbMax_ = {-1e9f, -1e9f, -1e9f};
    for (auto& rm : model->meshes_) {
        model->aabbMin_.x = std::fmin(model->aabbMin_.x, rm.aabbMin.x);
        model->aabbMin_.y = std::fmin(model->aabbMin_.y, rm.aabbMin.y);
        model->aabbMin_.z = std::fmin(model->aabbMin_.z, rm.aabbMin.z);
        model->aabbMax_.x = std::fmax(model->aabbMax_.x, rm.aabbMax.x);
        model->aabbMax_.y = std::fmax(model->aabbMax_.y, rm.aabbMax.y);
        model->aabbMax_.z = std::fmax(model->aabbMax_.z, rm.aabbMax.z);
    }
    if (model->aabbMin_.x > model->aabbMax_.x) {
        model->aabbMin_ = {-0.5f, -0.5f, -0.5f};
        model->aabbMax_ = {0.5f, 0.5f, 0.5f};
    }

    if (model->meshes_.empty()) {
        err = "glTF sem geometria utilizável";
        return nullptr;
    }
    return model;
}

// ---------------------------------------------------------------------------
// Amostragem de animação
// ---------------------------------------------------------------------------
void RuntimeModel::sampleSampler(const RuntimeAnimSampler& s, float t,
                                 float* out) const {
    const std::vector<float>& times = s.times;
    std::size_t n = times.size();
    if (n == 0) return;
    if (n == 1 || t <= times[0]) {
        for (int c = 0; c < s.comps; ++c) out[c] = s.values[c];
        return;
    }
    if (t >= times[n - 1]) {
        std::size_t base = (n - 1) * s.comps;
        for (int c = 0; c < s.comps; ++c) out[c] = s.values[base + c];
        return;
    }
    // busca binária do segmento
    std::size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) {
        std::size_t mid = (lo + hi) / 2;
        if (times[mid] <= t) lo = mid; else hi = mid;
    }
    float t0 = times[lo], t1 = times[hi];
    float alpha = (t1 - t0) > 1e-9f ? (t - t0) / (t1 - t0) : 0.0f;

    if (s.interpolation == GltfInterpolation::STEP) alpha = alpha > 0.5f ? 1.0f : 0.0f;

    if (s.interpolation == GltfInterpolation::CUBICSPLINE) {
        // layout: [in_tangent, value, out_tangent] por keyframe
        std::size_t stride = s.comps * 3;
        std::size_t v0 = lo * stride + s.comps;
        std::size_t v1 = hi * stride + s.comps;
        std::size_t outTan0 = lo * stride + s.comps * 2;
        std::size_t inTan1 = hi * stride;
        for (int c = 0; c < s.comps; ++c) {
            float p0 = s.values[v0 + c], p1 = s.values[v1 + c];
            float m0 = s.values[outTan0 + c], m1 = s.values[inTan1 + c];
            float dt = t1 - t0;
            float h00 = 2 * alpha * alpha * alpha - 3 * alpha * alpha + 1;
            float h10 = alpha * alpha * alpha - 2 * alpha * alpha + alpha;
            float h01 = -2 * alpha * alpha * alpha + 3 * alpha * alpha;
            float h11 = alpha * alpha * alpha - alpha * alpha;
            out[c] = h00 * p0 + h10 * dt * m0 + h01 * p1 + h11 * dt * m1;
        }
        return;
    }

    // LINEAR
    if (s.comps == 4) {
        // interpolação como quaternion
        Quat q0{s.values[lo * 4], s.values[lo * 4 + 1], s.values[lo * 4 + 2], s.values[lo * 4 + 3]};
        Quat q1{s.values[hi * 4], s.values[hi * 4 + 1], s.values[hi * 4 + 2], s.values[hi * 4 + 3]};
        Quat q = Quat::slerp(q0, q1, alpha);
        out[0] = q.x; out[1] = q.y; out[2] = q.z; out[3] = q.w;
    } else {
        for (int c = 0; c < s.comps; ++c)
            out[c] = lerpf(s.values[lo * s.comps + c], s.values[hi * s.comps + c], alpha);
    }
}

void RuntimeModel::applyAnimation(float t) {
    if (activeAnim_ < 0 || activeAnim_ >= animationCount()) return;
    const RuntimeAnimation& anim = animations_[activeAnim_];
    float at = anim.duration > 0 ? (loop_ ? std::fmod(t, anim.duration) : std::fmin(t, anim.duration)) : 0;

    for (const RuntimeAnimChannel& ch : anim.channels) {
        if (ch.node < 0 || ch.node >= static_cast<int>(nodes_.size())) continue;
        if (ch.sampler < 0 || ch.sampler >= static_cast<int>(anim.samplers.size())) continue;
        const RuntimeAnimSampler& s = anim.samplers[ch.sampler];
        Node& n = nodes_[ch.node];
        float out[16] = {0};
        sampleSampler(s, at, out);
        switch (ch.path) {
            case GltfAnimPath::TRANSLATION:
                n.translation = {out[0], out[1], out[2]};
                n.animPosition = true;
                break;
            case GltfAnimPath::ROTATION:
                n.rotation = Quat{out[0], out[1], out[2], out[3]}.normalized();
                n.animRotation = true;
                break;
            case GltfAnimPath::SCALE:
                n.scale = {out[0], out[1], out[2]};
                n.animScale = true;
                break;
        }
    }
}

void RuntimeModel::updateNodeWorld(int nodeIdx, const Mat4& parentWorld) {
    Node& n = nodes_[nodeIdx];
    Mat4 local = Mat4::translation(n.translation) * Mat4::rotation(n.rotation) *
                 Mat4::scale(n.scale);
    n.localMatrix = local;
    n.worldMatrix = parentWorld * local;
    for (int c : n.children)
        if (c >= 0 && c < static_cast<int>(nodes_.size()))
            updateNodeWorld(c, n.worldMatrix);
}

void RuntimeModel::update(float dtSec) {
    lastDt_ = dtSec;
    animTime_ += dtSec;
    if (activeAnim_ >= 0) applyAnimation(animTime_);
    (void)lastDt_;

    Mat4 identity = Mat4::identity();
    for (int r : rootNodes_) {
        if (r >= 0 && r < static_cast<int>(nodes_.size()))
            updateNodeWorld(r, identity);
    }

    // skinning: jointMatrix = world * IBM
    for (auto& skin : skins_) {
        for (std::size_t j = 0; j < skin.jointNodes.size(); ++j) {
            int node = skin.jointNodes[j];
            if (node >= 0 && node < static_cast<int>(nodes_.size())) {
                skin.jointMatrices[j] = nodes_[node].worldMatrix *
                                        skin.inverseBindMatrices[j];
            } else {
                skin.jointMatrices[j] = Mat4::identity();
            }
        }
    }
}

std::size_t RuntimeModel::memoryBytes() const {
    std::size_t total = sizeof(*this);
    for (auto& m : meshes_) {
        total += (m.positions.size() + m.normals.size() + m.uvs.size() +
                  m.joints.size() + m.weights.size()) * sizeof(float) +
                 m.indices.size() * sizeof(uint32_t);
    }
    for (auto& im : images_) total += im.bytes.size();
    for (auto& a : animations_) {
        for (auto& s : a.samplers)
            total += (s.times.size() + s.values.size()) * sizeof(float);
    }
    for (auto& sk : skins_)
        total += sk.jointMatrices.size() * 64 + sk.inverseBindMatrices.size() * 64;
    return total;
}

} // namespace brazilmr
