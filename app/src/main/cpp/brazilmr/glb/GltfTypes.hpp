// BrazilMR — tipos de dados do glTF 2.0 (subset suportado pelo runtime).
#pragma once

#include "../math/MathTypes.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace brazilmr {

enum class GltfComponentType : int {
    BYTE           = 5120,
    UNSIGNED_BYTE  = 5121,
    SHORT          = 5122,
    UNSIGNED_SHORT = 5123,
    UNSIGNED_INT   = 5125,
    FLOAT          = 5126,
};

enum class GltfAnimPath : int { TRANSLATION, ROTATION, SCALE };
enum class GltfInterpolation : int { LINEAR, STEP, CUBICSPLINE };
enum class GltfAlphaMode : int { OPAQUE, MASK, BLEND };

struct GltfBufferView {
    int64_t buffer = -1;
    int64_t byteOffset = 0;
    int64_t byteLength = 0;
    int64_t byteStride = 0;   // 0 = tightly packed
    int target = 0;           // 34962 ARRAY_BUFFER, 34963 ELEMENT_ARRAY_BUFFER
};

struct GltfAccessor {
    GltfComponentType componentType = GltfComponentType::FLOAT;
    bool normalized = false;
    int64_t count = 0;
    std::string type;          // "SCALAR" | "VEC2" | "VEC3" | "VEC4" | "MAT4"
    int64_t bufferView = -1;
    int64_t byteOffset = 0;
    float min[3] = {0, 0, 0};
    float max[3] = {0, 0, 0};
    bool hasMinMax = false;
    int numComponents() const {
        if (type == "SCALAR") return 1;
        if (type == "VEC2") return 2;
        if (type == "VEC3") return 3;
        if (type == "VEC4" || type == "MAT2") return 4;
        if (type == "MAT3") return 9;
        if (type == "MAT4") return 16;
        return 0;
    }
};

struct GltfPrimitive {
    static constexpr int kMaxAttributes = 8;
    struct Attr { const char* name; int accessor; };
    int attributes[kMaxAttributes][2] = {{0, -1}}; // [][0]=nome codificado, [][1]=accessor
    // nomes codificados: 0=POSITION 1=NORMAL 2=TEXCOORD_0 3=JOINTS_0 4=WEIGHTS_0
    int attrCount = 0;
    int indices = -1;
    int material = -1;
    int mode = 4; // TRIANGLES
};

struct GltfMesh {
    std::string name;
    std::vector<GltfPrimitive> primitives;
};

struct GltfNode {
    std::string name;
    int mesh = -1;
    int skin = -1;
    std::vector<int> children;
    bool hasMatrix = false;
    float matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float translation[3] = {0, 0, 0};
    float rotation[4] = {0, 0, 0, 1};
    float scale[3] = {1, 1, 1};
};

struct GltfPbrMaterial {
    float baseColorFactor[4] = {1, 1, 1, 1};
    int baseColorTexture = -1;      // índice de texture
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float emissiveFactor[3] = {0, 0, 0};
    GltfAlphaMode alphaMode = GltfAlphaMode::OPAQUE;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
};

struct GltfTexture {
    int sampler = -1;
    int source = -1;   // índice de imagem
};

struct GltfImage {
    std::string name;
    int bufferView = -1;
    std::string mimeType;
    std::string uri;   // data: URIs são suportados (base64); arquivos externos não
};

struct GltfSampler {
    int magFilter = 9729;   // LINEAR
    int minFilter = 9987;   // LINEAR_MIPMAP_LINEAR
    int wrapS = 10497;      // REPEAT
    int wrapT = 10497;
};

struct GltfAnimSampler {
    int input = -1;         // accessor de tempos
    int output = -1;        // accessor de valores
    GltfInterpolation interpolation = GltfInterpolation::LINEAR;
};

struct GltfAnimChannel {
    int sampler = -1;
    int targetNode = -1;
    GltfAnimPath path = GltfAnimPath::TRANSLATION;
};

struct GltfAnimation {
    std::string name;
    std::vector<GltfAnimChannel> channels;
    std::vector<GltfAnimSampler> samplers;
};

struct GltfSkin {
    std::string name;
    std::vector<int> joints;            // índices de nós
    int inverseBindMatrices = -1;       // accessor (MAT4)
    int skeleton = -1;
};

// Modelo completo decodificado.
struct GltfModel {
    std::vector<GltfBufferView> bufferViews;
    std::vector<GltfAccessor> accessors;
    std::vector<GltfMesh> meshes;
    std::vector<GltfNode> nodes;
    std::vector<GltfPbrMaterial> materials;
    std::vector<GltfTexture> textures;
    std::vector<GltfImage> images;
    std::vector<GltfSampler> samplers;
    std::vector<GltfAnimation> animations;
    std::vector<GltfSkin> skins;
    std::vector<int> sceneNodes;        // raízes da cena default
    std::string lastError;

    // Blob binário (chunk BIN do GLB) — todas as bufferViews apontam para cá.
    std::vector<uint8_t> binary;
};

// Parser do container GLB (binary glTF 2.0).
bool parseGlb(const uint8_t* data, std::size_t size, GltfModel& out);

// Parser de .gltf (JSON). Buffers externos (.bin) não são suportados;
// data URIs (base64) sim. Use GLB sempre que possível.
bool parseGltfJson(const char* json, std::size_t len, GltfModel& out,
                   std::vector<uint8_t> embeddedBuffer);

// Decodifica um accessor para floats (aplica normalização quando necessário).
bool decodeAccessorFloat(const GltfModel& m, int accessorIdx,
                         std::vector<float>& out);

// Decodifica um accessor para uint32 (índices).
bool decodeAccessorUint(const GltfModel& m, int accessorIdx,
                        std::vector<uint32_t>& out);

// Acesso direto ao byte range de um bufferView (sem cópia).
struct BufferSpan {
    const uint8_t* data = nullptr;
    std::size_t size = 0;
};
BufferSpan bufferViewSpan(const GltfModel& m, int bufferViewIdx);

} // namespace brazilmr
