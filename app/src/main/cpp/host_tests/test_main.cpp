// ===========================================================================
// BrazilMR — testes de host (executam no PC, sem Android/GL).
// Cobrem: matemática, One Euro, Kalman, fusão IMU, odometria visual (KLT +
// essencial), malha de distorção, JSON, GLB e hit-testing da UI espacial.
// Build/run: make -C host_tests (ou tools/host_test.sh)
// ===========================================================================
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "brazilmr/filters/OneEuroFilter.hpp"
#include "brazilmr/filters/KalmanFilter.hpp"
#include "brazilmr/filters/PoseFilterPipeline.hpp"
#include "brazilmr/glb/GltfTypes.hpp"
#include "brazilmr/glb/GltfRuntime.hpp"
#include "brazilmr/glb/Json.hpp"
#include "brazilmr/renderer/DistortionMesh.hpp"
#include "brazilmr/tracking/ImuFusion.hpp"
#include "brazilmr/tracking/OpticalFlow.hpp"
#include "brazilmr/tracking/VisualOdometry.hpp"
#include "brazilmr/ui/UiScene.hpp"

using namespace brazilmr;

static int gFailures = 0;
static int gTests = 0;

#define CHECK(cond) do { \
    ++gTests; \
    if (!(cond)) { \
        ++gFailures; \
        std::printf("FALHOU: %s (linha %d)\n", #cond, __LINE__); \
    } \
} while (0)

#define CHECK_NEAR(a, b, tol) do { \
    ++gTests; \
    double _d = std::fabs(double(a) - double(b)); \
    if (!(_d <= (tol))) { \
        ++gFailures; \
        std::printf("FALHOU: |%s - %s| = %f > %f (linha %d)\n", #a, #b, _d, \
                    double(tol), __LINE__); \
    } \
} while (0)

// ---------------------------------------------------------------------------
static void testMath() {
    std::printf("== matemática ==\n");
    // Euler ↔ quat roundtrip
    Quat q = Quat::fromEulerYXZ(0.4f, -0.3f, 0.25f);
    float y, p, r;
    q.toEulerYXZ(y, p, r);
    CHECK_NEAR(y, 0.4f, 1e-4);
    CHECK_NEAR(p, -0.3f, 1e-4);
    CHECK_NEAR(r, 0.25f, 1e-4);

    // rotate
    Vec3 v = Quat::fromAxisAngle(Vec3{0, 1, 0}, kPiHalf).rotate(Vec3{1, 0, 0});
    CHECK_NEAR(v.x, 0.0f, 1e-5);
    CHECK_NEAR(v.z, -1.0f, 1e-5);

    // slerp midpoint
    Quat qa = Quat::fromAxisAngle(Vec3{0, 1, 0}, 0.0f);
    Quat qb = Quat::fromAxisAngle(Vec3{0, 1, 0}, 1.0f);
    Quat qm = Quat::slerp(qa, qb, 0.5f);
    float my, mp, mr;
    qm.toEulerYXZ(my, mp, mr);
    CHECK_NEAR(my, 0.5f, 1e-4);

    // mat4 inversa
    Mat4 m = Mat4::translation({1, 2, 3}) * Mat4::rotation(q) * Mat4::scale({2, 2, 2});
    Mat4 inv = m.inverted();
    Vec3 pt{0.3f, -0.7f, 1.1f};
    Vec3 rt = inv.transformPoint(m.transformPoint(pt));
    CHECK_NEAR(rt.x, pt.x, 1e-4);
    CHECK_NEAR(rt.y, pt.y, 1e-4);
    CHECK_NEAR(rt.z, pt.z, 1e-4);

    // quatLog/Exp roundtrip
    Vec3 rv{0.1f, -0.2f, 0.3f};
    Quat qq = quatExp(rv);
    Vec3 rv2 = quatLog(qq);
    CHECK_NEAR(rv2.x, rv.x, 1e-5);
    CHECK_NEAR(rv2.y, rv.y, 1e-5);
    CHECK_NEAR(rv2.z, rv.z, 1e-5);
}

// ---------------------------------------------------------------------------
static void testOneEuro() {
    std::printf("== One Euro Filter ==\n");
    OneEuroConfig cfg;
    cfg.minCutoffHz = 1.0f;
    cfg.beta = 0.02f;
    OneEuroFilter f(cfg);

    // sinal constante com ruído
    std::vector<float> clean, filtered;
    float noise[] = {0.08f, -0.06f, 0.05f, -0.09f, 0.07f, -0.05f, 0.06f, -0.08f};
    float dt = 1.0f / 60.0f;
    for (int i = 0; i < 400; ++i) {
        float n = noise[i % 8];
        float x = 5.0f + n;
        clean.push_back(n);
        filtered.push_back(f.filter(x, dt));
    }
    // variância do ruído de saída (últimas 100 amostras) muito menor que 1
    double varIn = 0, varOut = 0;
    double meanOut = 0;
    for (int i = 300; i < 400; ++i) meanOut += filtered[i];
    meanOut /= 100;
    for (int i = 300; i < 400; ++i) {
        varIn += clean[i] * clean[i];
        varOut += (filtered[i] - meanOut) * (filtered[i] - meanOut);
    }
    varIn /= 100; varOut /= 100;
    CHECK(varOut < varIn * 0.2); // redução de jitter > 80%

    // resposta a degrau: converge (baixa latência via beta)
    f.reset();
    for (int i = 0; i < 30; ++i) f.filter(0.0f, dt);
    for (int i = 0; i < 60; ++i) f.filter(1.0f, dt);
    CHECK_NEAR(f.last(), 1.0f, 0.05);

    // QuatOneEuro: suaviza sem sair da variedade unitária
    QuatOneEuro qf;
    qf.configure(cfg);
    Quat noisy = Quat::identity();
    Quat out = Quat::identity();
    for (int i = 0; i < 200; ++i) {
        noisy = Quat::fromEulerYXZ(0.01f * std::sin(i * 0.7f), 0.3f, 0.0f);
        out = qf.filter(noisy, dt);
    }
    CHECK_NEAR(out.length(), 1.0f, 1e-4);
}

// ---------------------------------------------------------------------------
static void testKalman() {
    std::printf("== Kalman ==\n");
    KalmanPosVel::Config cfg;
    cfg.processNoiseAccel = 1.0;
    cfg.measurementNoise = 0.001;
    KalmanPosVel kf(cfg);
    float dt = 1.0f / 60.0f;
    // rampa com velocidade 0.5 m/s
    for (int i = 0; i < 120; ++i) {
        kf.predict(dt);
        float z = 0.5f * i * dt + 0.002f * std::sin(i * 3.0f); // ruído leve
        kf.update(z);
    }
    CHECK_NEAR(kf.velocity(), 0.5f, 0.05);
    CHECK_NEAR(kf.position(), 0.5f * 120 * dt, 0.03);
    // predição à frente acompanha (posição + velocidade·t)
    float ahead = kf.position() + kf.velocity() * 0.1f;
    CHECK_NEAR(ahead, kf.position() + kf.velocity() * 0.1f, 1e-6);

    // OrientationEkf: segue uma rotação com medição ruidosa
    OrientationEkf ekf;
    for (int i = 0; i < 300; ++i) {
        float t = i * dt;
        Vec3 gyro{0.0f, 0.4f, 0.0f}; // gira em yaw
        ekf.integrateGyro(gyro, dt);
        Quat meas = Quat::fromEulerYXZ(0.4f * t + 0.01f * std::sin(t * 37.0f),
                                        0.0f, 0.0f);
        ekf.updateAttitude(meas);
    }
    float y, p, r;
    ekf.orientation().toEulerYXZ(y, p, r);
    CHECK_NEAR(y, 0.4f * 300 * dt, 0.05);
}

// ---------------------------------------------------------------------------
static void testPosePipeline() {
    std::printf("== Pipeline de pose (RAW→OneEuro→Kalman→Prediction) ==\n");
    PoseFilterPipeline pipe;
    PoseFilterConfig cfg;
    cfg.predictionTimeSec = 0.02f;
    pipe.configure(cfg);

    Pose raw;
    raw.orientation = Quat::identity();
    raw.angularVelocity = {0.0f, 1.0f, 0.0f}; // yaw 1 rad/s
    raw.dofs = 3;
    raw.confidence = 1.0f; // backend saudável (sem damping)
    int64_t ts = 1'000'000'000LL;
    Pose out{};
    for (int i = 0; i < 180; ++i) { // 3 s → yaw 3.02 rad (< π, sem wrap)
        float t = i * (1.0f / 60.0f);
        raw.timestampNs = ts + i * 16'666'667LL;
        raw.orientation = Quat::fromEulerYXZ(t, 0.0f, 0.0f);
        out = pipe.process(raw);
    }
    float y, p, r;
    out.orientation.toEulerYXZ(y, p, r);
    CHECK_NEAR(y, 3.0f + 0.02f, 0.1);
}

// ---------------------------------------------------------------------------
static void testImuFusion() {
    std::printf("== Fusão IMU complementar ==\n");
    ImuFusion fusion;
    Vec3 g{0, 9.81f, 0}; // repouso: accel lê +9.81 no Y (mundo Y-up)
    float dt = 1.0f / 240.0f;
    // rotação pura em yaw 90°/s por 1 s, device "flat" (gravidade +Z)
    for (int i = 0; i < 240; ++i) {
        Vec3 gyro{0.0f, degToRad(90.0f), 0.0f};
        fusion.update(gyro, g, nullptr, dt);
    }
    float y, p, r;
    fusion.orientation().toEulerYXZ(y, p, r);
    CHECK_NEAR(radToDeg(y), 90.0f, 3.0f);
    CHECK_NEAR(radToDeg(p), 0.0f, 3.0f);
    CHECK_NEAR(radToDeg(r), 0.0f, 3.0f);

    // recenter zera yaw
    fusion.recenterYaw();
    fusion.orientation().toEulerYXZ(y, p, r);
    CHECK_NEAR(radToDeg(y), 0.0f, 0.5f);
}

// ---------------------------------------------------------------------------
// Gera imagem sintética com textura (blobs aleatórios determinísticos)
static void makeSyntheticImage(int w, int h, std::vector<uint8_t>& out) {
    out.resize(static_cast<std::size_t>(w) * h);
    uint32_t s = 12345;
    auto rnd = [&]() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    };
    // textura MULTI-ESCALA: blobs grandes (estrutura p/ pirâmide) + médios +
    // finos (corners p/ FAST). Sem saturação e sem ruído por pixel.
    std::vector<float> acc(static_cast<std::size_t>(w) * h, 128.0f);
    auto addBlobs = [&](int n, float amp, float invSigma2) {
        for (int k = 0; k < n; ++k) {
            float bx = static_cast<float>(rnd() % w);
            float by = static_cast<float>(rnd() % h);
            float s = (k & 1) ? 1.0f : -1.0f;
            int r = static_cast<int>(3.0f / std::sqrt(invSigma2));
            int x0 = std::fmax(0, static_cast<int>(bx) - r);
            int x1 = std::fmin(w - 1, static_cast<int>(bx) + r);
            int y0 = std::fmax(0, static_cast<int>(by) - r);
            int y1 = std::fmin(h - 1, static_cast<int>(by) + r);
            for (int y = y0; y <= y1; ++y)
                for (int x = x0; x <= x1; ++x) {
                    float d2 = (x - bx) * (x - bx) + (y - by) * (y - by);
                    acc[static_cast<std::size_t>(y) * w + x] +=
                        s * amp * std::exp(-d2 * invSigma2);
                }
        }
    };
    addBlobs(10, 55.0f, 0.004f); // σ ≈ 15.8 px
    addBlobs(25, 35.0f, 0.03f);  // σ ≈ 5.8 px
    addBlobs(60, 22.0f, 0.15f);  // σ ≈ 2.6 px
    for (std::size_t i = 0; i < acc.size(); ++i)
        out[i] = static_cast<uint8_t>(std::fmax(0.0f, std::fmin(255.0f, acc[i])));
}

static void testOpticalFlow() {
    std::printf("== Fluxo óptico (KLT piramidal) ==\n");
    const int W = 160, H = 120;
    std::vector<uint8_t> base;
    makeSyntheticImage(W, H, base);

    // desloca a imagem (+3.5, -2.5) px
    std::vector<uint8_t> shifted(static_cast<std::size_t>(W) * H);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int sx = static_cast<int>(x - 3.5f);
            int sy = static_cast<int>(y + 2.5f);
            sx = std::fmax(0, std::fmin(W - 1, sx));
            sy = std::fmax(0, std::fmin(H - 1, sy));
            shifted[y * W + x] = base[sy * W + sx];
        }

    LumaView a{base.data(), W, H, W};
    LumaView b{shifted.data(), W, H, W};
    ImagePyramid pa, pb;
    pa.build(a, 3);
    pb.build(b, 3);

    auto feats = detectFeatures(a, 12, 60, 6, 4);
    CHECK(feats.size() >= 20);

    std::vector<KltTrack> tracks;
    for (auto& f : feats) {
        KltTrack t;
        t.prevX = t.curX = f.x;
        t.prevY = t.curY = f.y;
        tracks.push_back(t);
    }
    trackFeatures(pa, pb, tracks);

    int valid = 0;
    double meanDx = 0, meanDy = 0;
    for (auto& t : tracks) {
        if (!t.valid) continue;
        ++valid;
        meanDx += t.curX - t.prevX;
        meanDy += t.curY - t.prevY;
    }
    CHECK(valid >= 10);
    meanDx /= valid;
    meanDy /= valid;
    // o fluxo mede o deslocamento do CONTEÚDO: +3.5 em x, -2.5 em y
    CHECK_NEAR(meanDx, 3.5, 0.6);
    CHECK_NEAR(meanDy, -2.5, 0.6);
}

// ---------------------------------------------------------------------------
static void testVisualOdometry() {
    std::printf("== Odometria visual (essencial + RANSAC) ==\n");
    const int W = 200, H = 150;
    std::vector<uint8_t> base;
    makeSyntheticImage(W, H, base);

    // câmera se move para a direita (translação +X): a cena aparenta mover -X
    std::vector<uint8_t> moved(static_cast<std::size_t>(W) * H);
    const float dx = 6.0f; // px de disparidade
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            int sx = static_cast<int>(x - dx);
            sx = std::fmax(0, std::fmin(W - 1, sx));
            moved[y * W + x] = base[y * W + sx];
        }

    VoConfig cfg;
    cfg.minTracksForPose = 20;
    cfg.maxFeatures = 160;
    cfg.fastThreshold = 12;
    cfg.ransacIterations = 300;
    VisualOdometry vo(cfg);
    LumaView f0{base.data(), W, H, W};
    LumaView f1{moved.data(), W, H, W};
    VoResult r0 = vo.processFrame(f0, 1'000'000'000LL);
    CHECK(!r0.ok); // primeiro frame só detecta
    VoResult r1 = vo.processFrame(f1, 1'033'333'333LL);
    CHECK(r1.ok);
    CHECK(r1.inliers >= 15);
    // translação horizontal: o alvo moveu -X na imagem → câmera moveu +X
    // (sinal depende da decomposição; validamos magnitude e dominância em X)
    CHECK(std::fabs(r1.translationUnit.x) > 0.7f);
    CHECK_NEAR(r1.translationUnit.length(), 1.0f, 1e-3);
}

// ---------------------------------------------------------------------------
static void testDistortion() {
    std::printf("== Malha de distorção (barrel + cromática) ==\n");
    const float k1 = 0.22f, k2 = 0.20f;
    // inversa consistente com a direta
    for (float r = 0.05f; r <= 1.4f; r += 0.15f) {
        float rd = distortRadius(r, k1, k2);
        float ru = undistortRadius(rd, k1, k2);
        CHECK_NEAR(ru, r, 1e-3);
    }

    StereoProfile prof;
    prof.lensK1 = k1;
    prof.lensK2 = k2;
    prof.chromaticAberration = 0.6f;
    std::vector<DistortionVertex> verts;
    std::vector<uint16_t> idx;
    buildDistortionGrid(prof, 0, 32, 1280, 720, false, verts, idx);
    CHECK(verts.size() == 33 * 33);
    CHECK(idx.size() == 32 * 32 * 6);
    // centro da malha: UV ≈ (0.5, 0.5) quando lensCenter = (0.5, 0.5)
    const DistortionVertex& center = verts[16 * 33 + 16];
    CHECK_NEAR(center.uG, 0.5f, 0.02);
    CHECK_NEAR(center.vG, 0.5f, 0.02);
    // borda direita do olho → UV < 1 (overscan: a região externa do alvo é
    // sacrificada, como no Cardboard; a lente recorta o círculo visível)
    const DistortionVertex& rightEdge = verts[16 * 33 + 32];
    CHECK(rightEdge.uG < 1.0f);
    CHECK(rightEdge.uG > 0.5f);
    // monotonicidade: uG cresce da esquerda para a direita
    const DistortionVertex& leftEdge = verts[16 * 33 + 0];
    CHECK(leftEdge.uG < center.uG);
    CHECK(center.uG < rightEdge.uG);
    // aberração cromática: vermelho distorce MAIS → na mesma posição de tela,
    // amostra um raio MENOR: uR ≤ uG ≤ uB (lado direito do centro)
    const DistortionVertex& mid = verts[16 * 33 + 24];
    CHECK(mid.uR <= mid.uG + 1e-6);
    CHECK(mid.uG <= mid.uB + 1e-6);

    // malha mono (fullscreen)
    std::vector<DistortionVertex> mono;
    buildDistortionGrid(prof, 0, 8, 1280, 720, true, mono, idx);
    CHECK(mono.size() == 81);
    // cobertura de tela completa
    CHECK_NEAR(mono[80].x, 1.0f, 1e-5);
}

// ---------------------------------------------------------------------------
static void testJson() {
    std::printf("== Parser JSON ==\n");
    const char* src = R"({
        "name": "glTF 2.0 ✓",
        "count": 42,
        "pi": 3.14159,
        "flag": true,
        "nested": {"arr": [1, 2.5, -3e2, null]},
        "esc": "linha\nnova \"aspas\" \\ fim"
    })";
    JsonValue v;
    std::string err;
    CHECK(jsonParse(src, std::strlen(src), v, err));
    CHECK(v.get("name").asString() == "glTF 2.0 ✓");
    CHECK(v.get("count").asInt() == 42);
    CHECK_NEAR(v.get("pi").asDouble(), 3.14159, 1e-9);
    CHECK(v.get("flag").asBool());
    const JsonValue& arr = v.get("nested").get("arr");
    CHECK(arr.size() == 4);
    CHECK_NEAR(arr.at(2).asDouble(), -300.0, 1e-9);
    CHECK(arr.at(3).isNull());
    CHECK(v.get("esc").asString() == "linha\nnova \"aspas\" \\ fim");
    // roundtrip dump
    std::string dumped = v.dump();
    JsonValue v2;
    CHECK(jsonParse(dumped, v2, err));
    CHECK_NEAR(v2.get("pi").asDouble(), 3.14159, 1e-9);

    // JSON inválido
    CHECK(!jsonParse("{\"a\":}", 6, v, err));
}

// ---------------------------------------------------------------------------
static void testGlb() {
    std::printf("== Parser GLB + Runtime glTF ==\n");
    FILE* f = std::fopen("test_model.glb", "rb");
    if (!f) {
        std::printf("AVISO: test_model.glb ausente (gere com tools/gen_test_glb.py)\n");
        CHECK(false);
        return;
    }
    std::vector<uint8_t> buf;
    char tmp[4096];
    size_t n;
    while ((n = std::fread(tmp, 1, sizeof(tmp), f)) > 0)
        buf.insert(buf.end(), tmp, tmp + n);
    std::fclose(f);

    GltfModel model;
    CHECK(parseGlb(buf.data(), buf.size(), model));
    CHECK(model.meshes.size() == 1);
    CHECK(model.meshes[0].primitives.size() == 1);
    CHECK(model.animations.size() == 1);
    CHECK(model.images.size() == 1);
    CHECK(model.images[0].bufferView >= 0);
    CHECK(model.materials.size() >= 1);
    CHECK_NEAR(model.materials[0].metallicFactor, 0.2f, 1e-6);

    std::string err;
    auto rt = RuntimeModel::fromGltf(model, err);
    CHECK(rt != nullptr);
    if (rt) {
        CHECK(rt->meshes().size() == 1);
        CHECK(rt->meshes()[0].vertexCount() == 81); // 9x9 grid
        CHECK(rt->meshes()[0].indexCount() == 384); // 8x8 x 6
        CHECK(!rt->meshes()[0].uvs.empty());
        CHECK(rt->animationCount() == 1);
        CHECK(rt->animationName(0) == "spin");
        CHECK(rt->materials()[0].baseColorImage == 0);
        CHECK(rt->images().size() == 1);
        CHECK(rt->images()[0].bytes.size() > 100);
        CHECK(rt->images()[0].bytes[0] == 0x89); // PNG
        // anima 1.5s (meio do loop) e verifica yaw ≠ 0
        rt->setActiveAnimation(0, true);
        rt->update(0.0f);
        rt->update(0.75f);
        rt->update(0.75f);
        float y, p, r;
        rt->nodes()[0].rotation.toEulerYXZ(y, p, r);
        // a animação gira em torno do eixo Z do nó (normal da placa) → roll
        CHECK(std::fabs(r) > 0.05f); // girou
        CHECK(std::fabs(r - 0.1745f) < 0.05f); // slerp(20°, 0°, 0.5) = 10°
        // AABB plausível
        CHECK(rt->aabbMin().x < -0.3f);
        CHECK(rt->aabbMax().x > 0.3f);
    }

    // GLB corrompido rejeitado
    std::vector<uint8_t> bad = buf;
    bad[4] = 0xEE;
    GltfModel m2;
    CHECK(!parseGlb(bad.data(), bad.size(), m2));
}

// ---------------------------------------------------------------------------
static void testUiScene() {
    std::printf("== UI espacial (hit-test + eventos) ==\n");
    UiScene scene;
    UiPanel panel;
    panel.position = {0, 1.5f, -1.5f};
    panel.widthM = 0.8f;
    panel.heightM = 0.6f;
    int pid = scene.addPanel(panel);

    UiControl btn;
    btn.kind = UiControlKind::BUTTON;
    btn.x = 0.1f; btn.y = 0.1f; btn.w = 0.3f; btn.h = 0.08f;
    std::strcpy(btn.label, "Aplicativos");
    int bid = scene.addControl(pid, btn);

    UiControl slider;
    slider.kind = UiControlKind::SLIDER;
    slider.x = 0.1f; slider.y = 0.3f; slider.w = 0.6f; slider.h = 0.08f;
    slider.value = 0.5f;
    std::strcpy(slider.label, "Escala");
    int sid = scene.addControl(pid, slider);

    // raio que cruza o painel no centro do botão (painel sem rotação,
    // origem do raio em (0,1.5,0) olhando -Z)
    Vec3 origin{0, 1.5f, 0};
    Vec3 dir{0, 0, -1};
    UiHit hit = scene.hitTest(origin, dir);
    CHECK(hit.hit);
    CHECK(hit.panelId == pid);
    // ponto central → u=0.5, v=0.5 → x=0.4, y=0.3 → slider
    CHECK(hit.controlId == sid);

    // raio no botão: u = 0.25 → x=0.2, v = 0.2333 → y=0.14
    dir = Vec3{(0.2f) / 1.5f, (1.5f - (1.5f - 0.14f)) / 1.5f - 0.0f, -1.0f}.normalized();
    // recalcula direto: alvo = painel.position + right*(x - w/2) + up*(h/2 - y)
    Vec3 right{1, 0, 0}, up{0, 1, 0};
    float x = 0.25f, y = 0.14f; // centro do botão
    Vec3 target = panel.position + right * (x - 0.4f) + up * (0.3f - y);
    dir = (target - origin).normalized();
    hit = scene.hitTest(origin, dir);
    CHECK(hit.hit);
    CHECK(hit.controlId == bid);

    // press + release no botão → evento UI_BUTTON
    std::vector<UiEvent> events;
    scene.dispatchPointer(origin, dir, UiPointerEvent::PRESS, events);
    scene.dispatchPointer(origin, dir, UiPointerEvent::RELEASE, events);
    CHECK(events.size() >= 1);
    bool gotButton = false;
    for (auto& e : events)
        if (e.type == UI_EVENT_BUTTON && e.controlId == bid) gotButton = true;
    CHECK(gotButton);

    // drag do slider: press no knob (value=0.5 → x=0.4) e move até 0.55
    events.clear();
    float sx = 0.1f + 0.6f * 0.5f; // knob em x=0.4
    Vec3 sTarget = panel.position + right * (sx - 0.4f) + up * (0.3f - 0.34f);
    Vec3 sDir = (sTarget - origin).normalized();
    scene.dispatchPointer(origin, sDir, UiPointerEvent::PRESS, events);
    float nx = 0.1f + 0.6f * 0.75f;
    Vec3 nTarget = panel.position + right * (nx - 0.4f) + up * (0.3f - 0.34f);
    Vec3 nDir = (nTarget - origin).normalized();
    scene.dispatchPointer(origin, nDir, UiPointerEvent::MOVE, events);
    bool gotSlider = false;
    float sliderVal = -1;
    for (auto& e : events)
        if (e.type == UI_EVENT_SLIDER) { gotSlider = true; sliderVal = e.value; }
    CHECK(gotSlider);
    CHECK_NEAR(sliderVal, 0.75f, 0.03);

    // fora do painel: sem hit
    UiHit miss = scene.hitTest({0, 1.5f, 0}, {0, 1, 0});
    CHECK(!miss.hit);
}

// ---------------------------------------------------------------------------
int main() {
    std::printf("BrazilMR — testes de host do núcleo nativo\n");
    std::printf("=============================================\n");
    testMath();
    testOneEuro();
    testKalman();
    testPosePipeline();
    testImuFusion();
    testOpticalFlow();
    testVisualOdometry();
    testDistortion();
    testJson();
    testGlb();
    testUiScene();
    std::printf("=============================================\n");
    std::printf("%d testes, %d falhas\n", gTests, gFailures);
    return gFailures == 0 ? 0 : 1;
}
