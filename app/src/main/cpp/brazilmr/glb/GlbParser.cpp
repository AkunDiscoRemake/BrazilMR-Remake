#include "GltfTypes.hpp"
#include "Json.hpp"
#include <cstring>

namespace brazilmr {

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
bool readU32(const uint8_t* p, uint32_t& out) {
    std::memcpy(&out, p, 4);
    return true;
}

int attrNameCode(const std::string& s) {
    if (s == "POSITION") return 0;
    if (s == "NORMAL") return 1;
    if (s == "TEXCOORD_0") return 2;
    if (s == "JOINTS_0") return 3;
    if (s == "WEIGHTS_0") return 4;
    return -1;
}

void parseNodeArray(const JsonValue& arr, std::vector<int>& out) {
    out.clear();
    if (!arr.isArray()) return;
    out.reserve(arr.size());
    for (std::size_t i = 0; i < arr.size(); ++i)
        out.push_back(arr.at(i).asInt(-1));
}

void parseVec3(const JsonValue& arr, float out[3]) {
    if (!arr.isArray()) return;
    out[0] = arr.at(0).asFloat(out[0]);
    out[1] = arr.at(1).asFloat(out[1]);
    out[2] = arr.at(2).asFloat(out[2]);
}

void parseVec4(const JsonValue& arr, float out[4]) {
    if (!arr.isArray()) return;
    out[0] = arr.at(0).asFloat(out[0]);
    out[1] = arr.at(1).asFloat(out[1]);
    out[2] = arr.at(2).asFloat(out[2]);
    out[3] = arr.at(3).asFloat(out[3]);
}

void parseBufferViews(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("bufferViews");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfBufferView bv;
        bv.buffer = jv.get("buffer").asInt(0);
        bv.byteOffset = jv.get("byteOffset").asInt(0);
        bv.byteLength = jv.get("byteLength").asInt(0);
        bv.byteStride = jv.get("byteStride").asInt(0);
        bv.target = jv.get("target").asInt(0);
        m.bufferViews.push_back(bv);
    }
}

void parseAccessors(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("accessors");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfAccessor acc;
        acc.componentType = static_cast<GltfComponentType>(jv.get("componentType").asInt(5126));
        acc.normalized = jv.get("normalized").asBool(false);
        acc.count = jv.get("count").asInt(0);
        acc.type = jv.get("type").asString();
        acc.bufferView = jv.get("bufferView").asInt(-1);
        acc.byteOffset = jv.get("byteOffset").asInt(0);
        const JsonValue& mn = jv.get("min");
        const JsonValue& mx = jv.get("max");
        if (mn.isArray() && mx.isArray()) {
            acc.hasMinMax = true;
            parseVec3(mn, acc.min);
            parseVec3(mx, acc.max);
        }
        m.accessors.push_back(acc);
    }
}

void parseMeshes(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("meshes");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfMesh mesh;
        mesh.name = jv.get("name").asString();
        const JsonValue& prims = jv.get("primitives");
        for (std::size_t p = 0; p < prims.size(); ++p) {
            const JsonValue& pj = prims.at(p);
            GltfPrimitive prim;
            prim.indices = pj.get("indices").asInt(-1);
            prim.material = pj.get("material").asInt(-1);
            prim.mode = pj.get("mode").asInt(4);
            const JsonValue& attrs = pj.get("attributes");
            if (attrs.isObject()) {
                // itera manualmente: JsonValue não expõe iteração — usamos nomes comuns
                static const char* kNames[] = {"POSITION", "NORMAL", "TEXCOORD_0",
                                               "JOINTS_0", "WEIGHTS_0"};
                for (const char* nm : kNames) {
                    const JsonValue& a = attrs.get(nm);
                    if (a.isNumber()) {
                        int code = attrNameCode(nm);
                        if (code >= 0 && prim.attrCount < GltfPrimitive::kMaxAttributes) {
                            prim.attributes[prim.attrCount][0] = code;
                            prim.attributes[prim.attrCount][1] = a.asInt(-1);
                            ++prim.attrCount;
                        }
                    }
                }
            }
            mesh.primitives.push_back(prim);
        }
        m.meshes.push_back(mesh);
    }
}

void parseNodes(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("nodes");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfNode node;
        node.name = jv.get("name").asString();
        node.mesh = jv.get("mesh").asInt(-1);
        node.skin = jv.get("skin").asInt(-1);
        parseNodeArray(jv.get("children"), node.children);
        const JsonValue& mat = jv.get("matrix");
        if (mat.isArray() && mat.size() == 16) {
            node.hasMatrix = true;
            for (int k = 0; k < 16; ++k) node.matrix[k] = mat.at(k).asFloat(0);
            node.matrix[15] = mat.at(15).asFloat(1);
        }
        parseVec3(jv.get("translation"), node.translation);
        parseVec4(jv.get("rotation"), node.rotation);
        parseVec3(jv.get("scale"), node.scale);
        m.nodes.push_back(node);
    }
}

void parseMaterials(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("materials");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfPbrMaterial mat;
        const JsonValue& pbr = jv.get("pbrMetallicRoughness");
        if (pbr.isObject()) {
            parseVec4(pbr.get("baseColorFactor"), mat.baseColorFactor);
            const JsonValue& bct = pbr.get("baseColorTexture");
            if (bct.isObject())
                mat.baseColorTexture = bct.get("index").asInt(-1);
            mat.metallicFactor = pbr.get("metallicFactor").asFloat(1.0f);
            mat.roughnessFactor = pbr.get("roughnessFactor").asFloat(1.0f);
        }
        parseVec3(jv.get("emissiveFactor"), mat.emissiveFactor);
        std::string alpha = jv.get("alphaMode").asString();
        if (alpha == "MASK") mat.alphaMode = GltfAlphaMode::MASK;
        else if (alpha == "BLEND") mat.alphaMode = GltfAlphaMode::BLEND;
        mat.alphaCutoff = jv.get("alphaCutoff").asFloat(0.5f);
        mat.doubleSided = jv.get("doubleSided").asBool(false);
        m.materials.push_back(mat);
    }
}

void parseImages(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("images");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfImage img;
        img.name = jv.get("name").asString();
        img.bufferView = jv.get("bufferView").asInt(-1);
        img.mimeType = jv.get("mimeType").asString();
        img.uri = jv.get("uri").asString();
        m.images.push_back(img);
    }
}

void parseAnimations(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("animations");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfAnimation anim;
        anim.name = jv.get("name").asString();
        const JsonValue& samplers = jv.get("samplers");
        for (std::size_t s = 0; s < samplers.size(); ++s) {
            const JsonValue& sj = samplers.at(s);
            GltfAnimSampler as;
            as.input = sj.get("input").asInt(-1);
            as.output = sj.get("output").asInt(-1);
            std::string interp = sj.get("interpolation").asString();
            if (interp == "STEP") as.interpolation = GltfInterpolation::STEP;
            else if (interp == "CUBICSPLINE") as.interpolation = GltfInterpolation::CUBICSPLINE;
            anim.samplers.push_back(as);
        }
        const JsonValue& channels = jv.get("channels");
        for (std::size_t c = 0; c < channels.size(); ++c) {
            const JsonValue& cj = channels.at(c);
            GltfAnimChannel ch;
            ch.sampler = cj.get("sampler").asInt(-1);
            const JsonValue& target = cj.get("target");
            if (target.isObject()) {
                ch.targetNode = target.get("node").asInt(-1);
                std::string path = target.get("path").asString();
                if (path == "rotation") ch.path = GltfAnimPath::ROTATION;
                else if (path == "scale") ch.path = GltfAnimPath::SCALE;
                else ch.path = GltfAnimPath::TRANSLATION;
            }
            anim.channels.push_back(ch);
        }
        m.animations.push_back(anim);
    }
}

void parseSkins(const JsonValue& root, GltfModel& m) {
    const JsonValue& arr = root.get("skins");
    for (std::size_t i = 0; i < arr.size(); ++i) {
        const JsonValue& jv = arr.at(i);
        GltfSkin skin;
        skin.name = jv.get("name").asString();
        parseNodeArray(jv.get("joints"), skin.joints);
        skin.inverseBindMatrices = jv.get("inverseBindMatrices").asInt(-1);
        skin.skeleton = jv.get("skeleton").asInt(-1);
        m.skins.push_back(skin);
    }
}

bool parseGltfJsonInternal(const JsonValue& root, GltfModel& m) {
    parseBufferViews(root, m);
    parseAccessors(root, m);
    parseMeshes(root, m);
    parseNodes(root, m);
    {
        const JsonValue& arr = root.get("materials");
        if (arr.isArray() && arr.size() == 0) m.materials.push_back(GltfPbrMaterial());
    }
    parseMaterials(root, m);
    if (m.materials.empty()) m.materials.push_back(GltfPbrMaterial());
    {
        const JsonValue& arr = root.get("textures");
        for (std::size_t i = 0; i < arr.size(); ++i) {
            const JsonValue& jv = arr.at(i);
            GltfTexture t;
            t.sampler = jv.get("sampler").asInt(-1);
            t.source = jv.get("source").asInt(-1);
            m.textures.push_back(t);
        }
    }
    parseImages(root, m);
    {
        const JsonValue& arr = root.get("samplers");
        for (std::size_t i = 0; i < arr.size(); ++i) {
            const JsonValue& jv = arr.at(i);
            GltfSampler s;
            s.magFilter = jv.get("magFilter").asInt(s.magFilter);
            s.minFilter = jv.get("minFilter").asInt(s.minFilter);
            s.wrapS = jv.get("wrapS").asInt(s.wrapS);
            s.wrapT = jv.get("wrapT").asInt(s.wrapT);
            m.samplers.push_back(s);
        }
    }
    parseAnimations(root, m);
    parseSkins(root, m);
    {
        int defaultScene = root.get("scene").asInt(0);
        const JsonValue& scenes = root.get("scenes");
        if (scenes.isArray()) {
            std::size_t idx = static_cast<std::size_t>(defaultScene);
            if (idx < scenes.size()) {
                parseNodeArray(scenes.at(idx).get("nodes"), m.sceneNodes);
            }
        }
        if (m.sceneNodes.empty() && !m.nodes.empty()) {
            // nós sem pai
            std::vector<bool> isChild(m.nodes.size(), false);
            for (auto& n : m.nodes)
                for (int c : n.children)
                    if (c >= 0 && c < static_cast<int>(isChild.size())) isChild[c] = true;
            for (std::size_t i = 0; i < m.nodes.size(); ++i)
                if (!isChild[i]) m.sceneNodes.push_back(static_cast<int>(i));
        }
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// GLB container
// ---------------------------------------------------------------------------
bool parseGlb(const uint8_t* data, std::size_t size, GltfModel& out) {
    if (size < 12) { out.lastError = "GLB muito pequeno"; return false; }
    uint32_t magic, version, length;
    readU32(data, magic);
    readU32(data + 4, version);
    readU32(data + 8, length);
    if (magic != 0x46546C67u) { out.lastError = "magic GLB inválido"; return false; }
    if (version != 2) { out.lastError = "versão GLB != 2"; return false; }
    if (length > size) { out.lastError = "tamanho GLB inconsistente"; return false; }

    std::size_t pos = 12;
    std::string jsonStr;
    const uint8_t* binData = nullptr;
    std::size_t binSize = 0;

    while (pos + 8 <= length) {
        uint32_t chunkLen, chunkType;
        readU32(data + pos, chunkLen);
        readU32(data + pos + 4, chunkType);
        pos += 8;
        if (pos + chunkLen > length) { out.lastError = "chunk GLB estoura o arquivo"; return false; }
        if (chunkType == 0x4E4F534Au) { // "JSON"
            jsonStr.assign(reinterpret_cast<const char*>(data + pos), chunkLen);
        } else if (chunkType == 0x004E4942u) { // "BIN"
            binData = data + pos;
            binSize = chunkLen;
        }
        pos += chunkLen;
        // chunks são alinhados a 4 bytes (padding de espaço)
        while (pos < length && (pos & 3) && pos < size) ++pos;
    }

    if (jsonStr.empty()) { out.lastError = "GLB sem chunk JSON"; return false; }

    JsonValue root;
    std::string err;
    if (!jsonParse(jsonStr, root, err)) {
        out.lastError = "JSON do glTF inválido: " + err;
        return false;
    }

    parseGltfJsonInternal(root, out);
    if (binData && binSize > 0) {
        out.binary.assign(binData, binData + binSize);
    }
    return true;
}

bool parseGltfJson(const char* json, std::size_t len, GltfModel& out,
                   std::vector<uint8_t> embeddedBuffer) {
    JsonValue root;
    std::string err;
    if (!jsonParse(json, len, root, err)) {
        out.lastError = "JSON do glTF inválido: " + err;
        return false;
    }
    parseGltfJsonInternal(root, out);
    out.binary = std::move(embeddedBuffer);
    return true;
}

// ---------------------------------------------------------------------------
// Decodificação de accessors
// ---------------------------------------------------------------------------
BufferSpan bufferViewSpan(const GltfModel& m, int bufferViewIdx) {
    BufferSpan span;
    if (bufferViewIdx < 0 || bufferViewIdx >= static_cast<int>(m.bufferViews.size()))
        return span;
    const GltfBufferView& bv = m.bufferViews[bufferViewIdx];
    if (bv.buffer != 0) return span; // só buffer 0 (GLB embutido)
    if (bv.byteOffset + bv.byteLength > static_cast<int64_t>(m.binary.size()))
        return span;
    span.data = m.binary.data() + bv.byteOffset;
    span.size = static_cast<std::size_t>(bv.byteLength);
    return span;
}

static std::size_t componentSize(GltfComponentType t) {
    switch (t) {
        case GltfComponentType::BYTE:
        case GltfComponentType::UNSIGNED_BYTE: return 1;
        case GltfComponentType::SHORT:
        case GltfComponentType::UNSIGNED_SHORT: return 2;
        case GltfComponentType::UNSIGNED_INT:
        case GltfComponentType::FLOAT: return 4;
    }
    return 0;
}

static float decodeComponent(const uint8_t* p, GltfComponentType t, bool normalized) {
    switch (t) {
        case GltfComponentType::FLOAT: {
            float f; std::memcpy(&f, p, 4); return f;
        }
        case GltfComponentType::BYTE: {
            int8_t v; std::memcpy(&v, p, 1);
            return normalized ? std::fmax(static_cast<float>(v) / 127.0f, -1.0f) : static_cast<float>(v);
        }
        case GltfComponentType::UNSIGNED_BYTE: {
            uint8_t v; std::memcpy(&v, p, 1);
            return normalized ? static_cast<float>(v) / 255.0f : static_cast<float>(v);
        }
        case GltfComponentType::SHORT: {
            int16_t v; std::memcpy(&v, p, 2);
            return normalized ? std::fmax(static_cast<float>(v) / 32767.0f, -1.0f) : static_cast<float>(v);
        }
        case GltfComponentType::UNSIGNED_SHORT: {
            uint16_t v; std::memcpy(&v, p, 2);
            return normalized ? static_cast<float>(v) / 65535.0f : static_cast<float>(v);
        }
        case GltfComponentType::UNSIGNED_INT: {
            uint32_t v; std::memcpy(&v, p, 4);
            return static_cast<float>(v);
        }
    }
    return 0.0f;
}

bool decodeAccessorFloat(const GltfModel& m, int accessorIdx,
                         std::vector<float>& out) {
    if (accessorIdx < 0 || accessorIdx >= static_cast<int>(m.accessors.size()))
        return false;
    const GltfAccessor& acc = m.accessors[accessorIdx];
    int comps = acc.numComponents();
    if (comps <= 0 || acc.count <= 0) return false;

    BufferSpan span;
    if (acc.bufferView >= 0) {
        span = bufferViewSpan(m, static_cast<int>(acc.bufferView));
        if (!span.data) return false;
    }

    std::size_t cs = componentSize(acc.componentType);
    if (!cs) return false;
    std::size_t elemBytes = cs * static_cast<std::size_t>(comps);
    std::size_t stride = elemBytes;
    if (acc.bufferView >= 0) {
        const GltfBufferView& bv = m.bufferViews[static_cast<std::size_t>(acc.bufferView)];
        if (bv.byteStride > 0) stride = static_cast<std::size_t>(bv.byteStride);
    }

    out.clear();
    out.reserve(static_cast<std::size_t>(acc.count) * comps);

    if (span.data) {
        const uint8_t* base = span.data + acc.byteOffset;
        for (int64_t i = 0; i < acc.count; ++i) {
            const uint8_t* elem = base + i * stride;
            for (int c = 0; c < comps; ++c)
                out.push_back(decodeComponent(elem + c * cs, acc.componentType, acc.normalized));
        }
    } else {
        // accessor sem bufferView → sparse não suportado; preenche zeros
        out.assign(static_cast<std::size_t>(acc.count) * comps, 0.0f);
    }
    return true;
}

bool decodeAccessorUint(const GltfModel& m, int accessorIdx,
                        std::vector<uint32_t>& out) {
    std::vector<float> tmp;
    if (!decodeAccessorFloat(m, accessorIdx, tmp)) return false;
    out.clear();
    out.reserve(tmp.size());
    for (float v : tmp) {
        float r = v < 0 ? 0 : v;
        out.push_back(static_cast<uint32_t>(r + 0.5f));
    }
    return true;
}

} // namespace brazilmr
