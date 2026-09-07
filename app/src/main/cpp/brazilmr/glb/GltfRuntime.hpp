// BrazilMR — runtime glTF: converte o modelo parseado em estruturas renderizáveis,
// aplica animações (TRS, LINEAR/STEP/CUBICSPLINE) e skinning (joints + IBM).
// Nenhuma chamada GL aqui: upload e draw ficam no ModelRenderer (camada gl/).
#pragma once

#include "../math/MathTypes.hpp"
#include "GltfTypes.hpp"
#include <memory>
#include <string>
#include <vector>

namespace brazilmr {

// Malha renderizável (uma por primitive).
struct RuntimeMesh {
    std::vector<float> positions;      // 3/vertex
    std::vector<float> normals;        // 3/vertex (pode ser vazia → flat-ish)
    std::vector<float> uvs;            // 2/vertex
    std::vector<float> joints;         // 4/vertex (skinning)
    std::vector<float> weights;        // 4/vertex (skinning)
    std::vector<uint32_t> indices;
    int material = 0;                  // índice em RuntimeModel::materials
    Vec3 aabbMin{0, 0, 0}, aabbMax{0, 0, 0};
    bool hasSkin = false;

    std::size_t vertexCount() const { return positions.size() / 3; }
    std::size_t indexCount() const { return indices.size(); }
};

// Índices de atributo de um primitive.
struct RuntimeNodeMesh {
    int meshIndex = -1;
};

struct RuntimeNode {
    std::string name;
    Vec3 translation{0, 0, 0};
    Quat rotation = Quat::identity();
    Vec3 scale{1, 1, 1};
    Mat4 localMatrix = Mat4::identity();
    Mat4 worldMatrix = Mat4::identity();
    std::vector<int> children;
    std::vector<RuntimeNodeMesh> meshes;
    int skin = -1;
    int parent = -1;
    // pose de animação aplicada em cima da pose bind
    bool animPosition = false, animRotation = false, animScale = false;
};

struct RuntimeMaterial {
    float baseColorFactor[4] = {1, 1, 1, 1};
    int baseColorImage = -1;       // índice em images (blobs)
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float emissiveFactor[3] = {0, 0, 0};
    GltfAlphaMode alphaMode = GltfAlphaMode::OPAQUE;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};

struct RuntimeImage {
    std::string name;
    std::string mimeType;          // "image/png" | "image/jpeg"
    std::vector<uint8_t> bytes;    // bytes do arquivo (decodificação é externa)
};

// Sampler de animação decodificado.
struct RuntimeAnimSampler {
    std::vector<float> times;
    std::vector<float> values;     // n comps por keyframe
    int comps = 1;
    GltfInterpolation interpolation = GltfInterpolation::LINEAR;
};

struct RuntimeAnimChannel {
    int node = -1;
    GltfAnimPath path = GltfAnimPath::TRANSLATION;
    int sampler = -1;
};

struct RuntimeAnimation {
    std::string name;
    std::vector<RuntimeAnimChannel> channels;
    std::vector<RuntimeAnimSampler> samplers;
    float duration = 0.0f;
};

struct RuntimeSkin {
    std::vector<int> jointNodes;              // nó (RuntimeNode) de cada joint
    std::vector<Mat4> inverseBindMatrices;
    std::vector<Mat4> jointMatrices;          // saída: world * IBM
};

// ---------------------------------------------------------------------------
// RuntimeModel
// ---------------------------------------------------------------------------
class RuntimeModel {
public:
    static std::shared_ptr<RuntimeModel> fromGltf(const GltfModel& gltf,
                                                  std::string& err);

    // Aplica animação (índice, -1 = nenhuma) e atualiza a hierarquia.
    void update(float timeSec);

    // Animações
    int animationCount() const { return static_cast<int>(animations_.size()); }
    std::string animationName(int i) const {
        return (i >= 0 && i < animationCount()) ? animations_[i].name : std::string();
    }
    void setActiveAnimation(int idx, bool loop = true) {
        activeAnim_ = idx;
        loop_ = loop;
        animTime_ = 0.0f;
    }
    int activeAnimation() const { return activeAnim_; }

    // Skinning pronto para upload (matrices em ordem de skin.jointNodes).
    const std::vector<RuntimeSkin>& skins() const { return skins_; }

    // Stats para o Performance/Memory Manager.
    std::size_t memoryBytes() const;

    const std::vector<RuntimeNode>& nodes() const { return nodes_; }
    const std::vector<RuntimeAnimation>& animations() const { return animations_; }
    const std::vector<RuntimeMesh>& meshes() const { return meshes_; }
    const std::vector<RuntimeMaterial>& materials() const { return materials_; }
    const std::vector<RuntimeImage>& images() const { return images_; }
    const std::vector<int>& rootNodes() const { return rootNodes_; }
    const Vec3& aabbMin() const { return aabbMin_; }
    const Vec3& aabbMax() const { return aabbMax_; }

    // Transform do modelo como um todo (espaço VR).
    Vec3 position{0, 0, 0};
    Quat orientation = Quat::identity();
    Vec3 scale{1, 1, 1};
    bool visible = true;
    uint64_t userId = 0;  // handle da API (Lua/C++)

    Mat4 modelMatrix() const {
        return Mat4::translation(position) * Mat4::rotation(orientation) *
               Mat4::scale(scale);
    }

private:
    void updateNodeWorld(int nodeIdx, const Mat4& parentWorld);
    void applyAnimation(float t);
    void sampleSampler(const RuntimeAnimSampler& s, float t, float* out) const;

    std::vector<RuntimeNode> nodes_;
    std::vector<RuntimeMesh> meshes_;
    std::vector<RuntimeMaterial> materials_;
    std::vector<RuntimeImage> images_;
    std::vector<RuntimeAnimation> animations_;
    std::vector<RuntimeSkin> skins_;
    std::vector<int> rootNodes_;
    Vec3 aabbMin_{0, 0, 0}, aabbMax_{0, 0, 0};

    int activeAnim_ = -1;
    bool loop_ = true;
    float animTime_ = 0.0f;
    float lastDt_ = 0.0f;
};

} // namespace brazilmr
