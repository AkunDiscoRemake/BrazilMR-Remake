#include "ModelRenderer.hpp"

namespace brazilmr {

static const int kMaxJoints = 48;

static const char* kVs = R"(#version 300 es
precision highp float;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 aJoints;
layout(location = 4) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uJointMat[48];
uniform float uUseSkin;

out vec3 vWorld;
out vec3 vNormal;
out vec2 vUv;

void main() {
    vec4 pos = vec4(aPos, 1.0);
    vec4 nrm = vec4(aNormal, 0.0);
    if (uUseSkin > 0.5) {
        mat4 skin =
            aWeights.x * uJointMat[int(aJoints.x)] +
            aWeights.y * uJointMat[int(aJoints.y)] +
            aWeights.z * uJointMat[int(aJoints.z)] +
            aWeights.w * uJointMat[int(aJoints.w)];
        pos = skin * pos;
        nrm = skin * nrm;
    }
    vec4 world = uModel * pos;
    vWorld = world.xyz;
    vNormal = mat3(uModel) * nrm.xyz;
    vUv = aUv;
    gl_Position = uViewProj * world;
}
)";

static const char* kFs = R"(#version 300 es
precision highp float;

in vec3 vWorld;
in vec3 vNormal;
in vec2 vUv;

uniform vec4 uBaseColor;
uniform float uMetallic;
uniform float uRoughness;
uniform vec3 uEmissive;
uniform sampler2D uBaseTex;
uniform float uHasBaseTex;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uCamPos;
uniform float uAlphaCutoff;

out vec4 outColor;

void main() {
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;
    vec3 V = normalize(uCamPos - vWorld);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(L + V);

    vec4 base = uBaseColor;
    if (uHasBaseTex > 0.5) base *= texture(uBaseTex, vUv);
    if (base.a < uAlphaCutoff) discard;

    float ndl = max(dot(N, L), 0.0);
    // ambiente hemisférico
    vec3 ambient = mix(vec3(0.10, 0.11, 0.14), vec3(0.16, 0.18, 0.24), N.y * 0.5 + 0.5);
    // especular GGX aproximado
    float a = max(uRoughness * uRoughness, 0.02);
    float a2 = a * a;
    float ndh = max(dot(N, H), 0.0);
    float d = (ndh * ndh * (a2 - 1.0) + 1.0);
    float spec = a2 / (3.14159 * d * d);
    float fres = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    vec3 specCol = mix(vec3(0.04), base.rgb, uMetallic);

    vec3 diffuse = base.rgb * ndl;
    vec3 color = base.rgb * ambient
               + diffuse * uLightColor * ndl
               + specCol * spec * uLightColor * ndl * (1.0 + fres * 2.0)
               + uEmissive;
    outColor = vec4(color, base.a);
}
)";

bool ModelRenderer::init() {
    if (!shader_.build(kVs, kFs)) return false;
    if (!aabbShader_.build(
            "#version 300 es\n"
            "precision highp float;\n"
            "layout(location=0) in vec3 aPos;\n"
            "uniform mat4 uMvp;\n"
            "void main() { gl_Position = uMvp * vec4(aPos, 1.0); }\n",
            "#version 300 es\n"
            "precision highp float;\n"
            "uniform vec4 uColor;\n"
            "out vec4 o;\n"
            "void main() { o = uColor; }\n")) {
        return false;
    }
    // cubo wireframe
    static const float edges[] = {
        -1,-1,-1, 1,-1,-1,  1,-1,-1, 1,1,-1,  1,1,-1, -1,1,-1,  -1,1,-1, -1,-1,-1,
        -1,-1, 1, 1,-1, 1,  1,-1, 1, 1,1, 1,  1,1, 1, -1,1, 1,  -1,1, 1, -1,-1, 1,
        -1,-1,-1, -1,-1,1,  1,-1,-1, 1,-1,1,  1,1,-1, 1,1,1,  -1,1,-1, -1,1,1
    };
    std::vector<VertexAttrib> attrs = {{3, GL_FLOAT, false}};
    if (!aabbMesh_.upload(edges, 24, 12, attrs, nullptr, 0, GL_UNSIGNED_SHORT))
        return false;
    ready_ = true;
    return true;
}

void ModelRenderer::destroy() {
    shader_.destroy();
    aabbShader_.destroy();
    aabbMesh_.destroy();
    gpuCache_.clear();
    ready_ = false;
}

bool ModelRenderer::uploadRuntimeMesh(const RuntimeMesh& rm, Mesh& out,
                                      bool& hasN, bool& hasUv) {
    // interleave: pos(3) normal(3) uv(2) joints(4) weights(4) = 16 floats
    const std::size_t vc = rm.vertexCount();
    if (vc == 0) return false;
    const bool n = !rm.normals.empty();
    const bool uv = !rm.uvs.empty();
    const bool sk = !rm.joints.empty() && !rm.weights.empty();
    hasN = n;   // reporta o layout ao caller (setup do VAO)
    hasUv = uv;

    std::vector<float> verts;
    verts.reserve(vc * 16);
    for (std::size_t i = 0; i < vc; ++i) {
        verts.push_back(rm.positions[i * 3]);
        verts.push_back(rm.positions[i * 3 + 1]);
        verts.push_back(rm.positions[i * 3 + 2]);
        verts.push_back(n ? rm.normals[i * 3] : 0.0f);
        verts.push_back(n ? rm.normals[i * 3 + 1] : 1.0f);
        verts.push_back(n ? rm.normals[i * 3 + 2] : 0.0f);
        verts.push_back(uv ? rm.uvs[i * 2] : 0.0f);
        verts.push_back(uv ? rm.uvs[i * 2 + 1] : 0.0f);
        if (sk) {
            verts.push_back(rm.joints[i * 4]);
            verts.push_back(rm.joints[i * 4 + 1]);
            verts.push_back(rm.joints[i * 4 + 2]);
            verts.push_back(rm.joints[i * 4 + 3]);
            verts.push_back(rm.weights[i * 4]);
            verts.push_back(rm.weights[i * 4 + 1]);
            verts.push_back(rm.weights[i * 4 + 2]);
            verts.push_back(rm.weights[i * 4 + 3]);
        } else {
            verts.push_back(0); verts.push_back(0); verts.push_back(0); verts.push_back(0);
            verts.push_back(1); verts.push_back(0); verts.push_back(0); verts.push_back(0);
        }
    }

    // indices: clamp joints para o limite do uniform
    std::vector<uint16_t> idx16;
    std::vector<uint32_t> idx32;
    const bool use32 = vc > 65535;
    if (use32) idx32 = rm.indices;
    else {
        idx16.reserve(rm.indices.size());
        for (uint32_t v : rm.indices) idx16.push_back(static_cast<uint16_t>(v));
    }

    std::vector<VertexAttrib> attrs = {
        {3, GL_FLOAT, false}, {3, GL_FLOAT, false}, {2, GL_FLOAT, false},
        {4, GL_FLOAT, false}, {4, GL_FLOAT, false},
    };
    const void* idxData = use32 ? static_cast<const void*>(idx32.data())
                                : static_cast<const void*>(idx16.data());
    return out.upload(verts.data(), vc, 16 * sizeof(float), attrs,
                      idxData, rm.indices.size(),
                      use32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT);
}

ModelRenderer::ModelGpu* ModelRenderer::gpuFor(
    const std::shared_ptr<RuntimeModel>& model, bool create) {
    auto it = gpuCache_.find(model.get());
    if (it != gpuCache_.end()) return &it->second;
    if (!create) return nullptr;

    ModelGpu gpu;
    gpu.meshes.reserve(model->meshes().size());
    for (auto& rm : model->meshes()) {
        GpuMesh gm;
        gm.material = rm.material;
        gm.hasNormals = !rm.normals.empty();
        gm.hasUv = !rm.uvs.empty();
        gm.vertexCount = rm.vertexCount();
        gm.indexCount = rm.indexCount();
        if (!uploadRuntimeMesh(rm, gm.mesh, gm.hasNormals, gm.hasUv)) continue;
        gpu.meshes.push_back(std::move(gm));
    }
    if (gpu.meshes.empty()) return nullptr;

    gpu.aabbMin = model->aabbMin();
    gpu.aabbMax = model->aabbMax();
    for (auto& s : model->skins()) {
        int jc = static_cast<int>(s.jointNodes.size());
        if (jc > gpu.skinJointCount) gpu.skinJointCount = jc;
    }
    if (gpu.skinJointCount > kMaxJoints) gpu.skinJointCount = kMaxJoints;

    gpuCache_[model.get()] = std::move(gpu);
    return &gpuCache_[model.get()];
}

bool ModelRenderer::registerModel(const std::shared_ptr<RuntimeModel>& model) {
    return gpuFor(model, true) != nullptr;
}

void ModelRenderer::unregisterModel(const std::shared_ptr<RuntimeModel>& model) {
    if (!model) return;
    auto it = gpuCache_.find(model.get());
    if (it != gpuCache_.end()) gpuCache_.erase(it);
}

void ModelRenderer::setModelTexture(const std::shared_ptr<RuntimeModel>& model,
                                    int imageIndex, GLuint texId) {
    if (!model) return;
    ModelGpu* gpu = gpuFor(model, false);
    if (!gpu) return;
    if (imageIndex >= 0) gpu->imageTextures[imageIndex] = texId;
}

void ModelRenderer::draw(const std::shared_ptr<RuntimeModel>& model,
                         const Camera& cam, const SceneLight& light, bool cull) {
    if (!ready_ || !model || !model->visible) return;
    ModelGpu* gpu = gpuFor(model, true);
    if (!gpu) return;

    const Mat4 modelMat = model->modelMatrix();

    // frustum culling no AABB do modelo
    if (cull) {
        Vec3 corners[8];
        const Vec3& mn = gpu->aabbMin;
        const Vec3& mx = gpu->aabbMax;
        int ci = 0;
        for (int a = 0; a < 2; ++a)
            for (int b = 0; b < 2; ++b)
                for (int c = 0; c < 2; ++c) {
                    Vec3 local{a ? mx.x : mn.x, b ? mx.y : mn.y, c ? mx.z : mn.z};
                    corners[ci++] = modelMat.transformPoint(local);
                }
        Vec3 wmn = corners[0], wmx = corners[0];
        for (int i = 1; i < 8; ++i) {
            wmn.x = std::fmin(wmn.x, corners[i].x);
            wmn.y = std::fmin(wmn.y, corners[i].y);
            wmn.z = std::fmin(wmn.z, corners[i].z);
            wmx.x = std::fmax(wmx.x, corners[i].x);
            wmx.y = std::fmax(wmx.y, corners[i].y);
            wmx.z = std::fmax(wmx.z, corners[i].z);
        }
        if (cam.cullAabb(wmn, wmx)) return;
    }

    shader_.use();
    shader_.setMat4("uModel", modelMat);
    shader_.setMat4("uViewProj", cam.viewProj);
    shader_.setVec3("uLightDir", light.direction);
    shader_.setVec3("uLightColor", light.color * light.intensity);
    shader_.setVec3("uCamPos", cam.position);

    const bool hasSkin = !model->skins().empty() && gpu->skinJointCount > 0;
    shader_.setFloat("uUseSkin", hasSkin ? 1.0f : 0.0f);
    if (hasSkin) {
        // paleta única (primeira skin) — suficiente p/ a maioria
        const RuntimeSkin& skin = model->skins().front();
        float palette[kMaxJoints * 16];
        int j = 0;
        for (; j < gpu->skinJointCount && j < static_cast<int>(skin.jointMatrices.size()); ++j) {
            const Mat4& jm = skin.jointMatrices[j];
            for (int k = 0; k < 16; ++k) palette[j * 16 + k] = jm.m[k];
        }
        GLint loc = shader_.uniform("uJointMat");
        if (loc >= 0) glUniformMatrix4fv(loc, j, GL_FALSE, palette);
    }

    const auto& materials = model->materials();
    const auto& meshes = model->meshes();

    // desenha por mesh do runtime (nodes referenciam por índice)
    // → para cada node com meshes, aplicar world matrix como parte do model
    // (simplificação: model matrix global * node world)
    for (const auto& node : model->nodes()) {
        for (const auto& nm : node.meshes) {
            if (nm.meshIndex < 0 || nm.meshIndex >= static_cast<int>(gpu->meshes.size()))
                continue;
            const GpuMesh& gm = gpu->meshes[nm.meshIndex];
            const RuntimeMesh& rmesh = meshes[nm.meshIndex];
            const RuntimeMaterial* mat = nullptr;
            if (gm.material >= 0 && gm.material < static_cast<int>(materials.size()))
                mat = &materials[gm.material];

            Mat4 fullMat = modelMat * node.worldMatrix;
            shader_.setMat4("uModel", fullMat);

            if (mat) {
                shader_.setVec4("uBaseColor", mat->baseColorFactor[0],
                                mat->baseColorFactor[1], mat->baseColorFactor[2],
                                mat->baseColorFactor[3]);
                shader_.setFloat("uMetallic", mat->metallicFactor);
                shader_.setFloat("uRoughness", clampf(mat->roughnessFactor, 0.05f, 1.0f));
                shader_.setVec3("uEmissive", Vec3{mat->emissiveFactor[0],
                                                  mat->emissiveFactor[1],
                                                  mat->emissiveFactor[2]});
                shader_.setFloat("uAlphaCutoff",
                                 mat->alphaMode == GltfAlphaMode::MASK ? mat->alphaCutoff : 0.0f);
                GLuint tex = 0;
                auto it = gpu->imageTextures.find(mat->baseColorImage);
                if (it != gpu->imageTextures.end()) tex = it->second;
                if (tex && mat->baseColorImage >= 0) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    shader_.setInt("uBaseTex", 0);
                    shader_.setFloat("uHasBaseTex", 1.0f);
                } else {
                    shader_.setFloat("uHasBaseTex", 0.0f);
                }
            } else {
                shader_.setVec4("uBaseColor", 0.8f, 0.8f, 0.8f, 1.0f);
                shader_.setFloat("uMetallic", 0.1f);
                shader_.setFloat("uRoughness", 0.6f);
                shader_.setVec3("uEmissive", Vec3{0, 0, 0});
                shader_.setFloat("uAlphaCutoff", 0.0f);
                shader_.setFloat("uHasBaseTex", 0.0f);
            }

            if (mat && mat->alphaMode == GltfAlphaMode::BLEND) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                gm.mesh.draw();
                glDisable(GL_BLEND);
            } else {
                gm.mesh.draw();
            }
            (void)rmesh;
        }
    }
}

void ModelRenderer::drawAabb(const Vec3& mn, const Vec3& mx, const Camera& cam) {
    Mat4 m = Mat4::translation((mn + mx) * 0.5f) *
             Mat4::scale((mx - mn) * 0.5f);
    Mat4 mvp = cam.viewProj * m;
    aabbShader_.use();
    aabbShader_.setMat4("uMvp", mvp);
    aabbShader_.setVec4("uColor", 0.0f, 0.9f, 0.63f, 1.0f);
    aabbMesh_.draw(GL_LINES);
}

} // namespace brazilmr
