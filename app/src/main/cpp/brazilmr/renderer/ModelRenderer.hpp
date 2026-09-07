// BrazilMR — renderizador de modelos GLB/glTF (PBR simplificado + skinning).
// Otimizações: cache de meshes por modelo, frustum culling por AABB e
// texturas por imagem (cache).
#pragma once

#include "../gl/GlUtils.hpp"
#include "../glb/GltfRuntime.hpp"
#include "Camera.hpp"
#include <map>
#include <memory>
#include <vector>

namespace brazilmr {

struct SceneLight {
    Vec3 direction{0.4f, -0.8f, 0.3f};  // mundo, normalizada
    Vec3 color{1.0f, 0.98f, 0.94f};
    float intensity = 1.0f;
};

class ModelRenderer {
public:
    bool init();
    void destroy();

    // Registra um modelo (upload de meshes). Retorna false se falhar.
    bool registerModel(const std::shared_ptr<RuntimeModel>& model);
    void unregisterModel(const std::shared_ptr<RuntimeModel>& model);

    // Associa textura decodificada (Kotlin decodifica PNG/JPG → RGBA → upload).
    void setModelTexture(const std::shared_ptr<RuntimeModel>& model,
                         int imageIndex, GLuint texId);

    // Draw de um modelo com a câmera dada (aplica frustum culling).
    void draw(const std::shared_ptr<RuntimeModel>& model, const Camera& cam,
              const SceneLight& light, bool cull = true);

    // Debug: AABB wireframe (futuro visualizador).
    void drawAabb(const Vec3& mn, const Vec3& mx, const Camera& cam);

private:
    struct GpuMesh {
        Mesh mesh;
        int material = 0;
        std::size_t vertexCount = 0;
        std::size_t indexCount = 0;
        bool hasNormals = false;
        bool hasUv = false;
    };
    struct ModelGpu {
        std::vector<GpuMesh> meshes;                 // ordem de RuntimeModel::meshes()
        std::map<int, GLuint> imageTextures;         // imageIndex → texture
        Vec3 aabbMin, aabbMax;
        int skinJointCount = 0;
    };

    bool uploadRuntimeMesh(const RuntimeMesh& rm, Mesh& out, bool& hasN, bool& hasUv);
    ModelGpu* gpuFor(const std::shared_ptr<RuntimeModel>& model, bool create);

    Shader shader_;
    Shader aabbShader_;
    Mesh aabbMesh_;
    bool ready_ = false;

    // cache por ponteiro de modelo (shared_ptr raw addr é estável)
    std::map<const RuntimeModel*, ModelGpu> gpuCache_;
};

} // namespace brazilmr
