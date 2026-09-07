#include "VR.hpp"
#include "../core/Clock.hpp"
#include "../core/Log.hpp"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace brazilmr {
namespace VR {

// ===========================================================================
// Impl singleton
// ===========================================================================
namespace {

struct VrEngineImpl {
    VrInitParams params;
    Headset headset;
    Tracking tracking;
    Hands hands;
    Windows windows;
    Scene scene;
    UiScene ui;
    Compositor compositor;
    StereoRenderer stereo;

    // renderers (GL thread)
    SkyRenderer sky;
    ModelRenderer models;
    WindowRenderer windowRenderer;
    HandRenderer handRenderer;
    UiRenderer uiRenderer;
    bool glReady = false;

    // ray ativo (gaze/controller/mão)
    std::mutex rayMutex;
    bool rayVisible = false;
    Vec3 rayOrigin{0, 0, 0}, rayDir{0, 0, -1}, rayColor{0, 0.9f, 0.63f};

    // eventos
    std::mutex eventMutex;
    std::vector<VrEvent> events;
    void (*eventCallback)(const VrEvent&) = nullptr;

    // performance
    float thermalBudget = 1.0f;
    EffectQuality fx;
    float frameMsEma = 16.6f;
    float fpsEst = 60.0f;
    int drawCalls = 0;
    int64_t lastFrameNs = 0;

    // late-latch
    std::mutex poseMutex;
    Quat qAtSceneRender = Quat::identity();

    void queueEvent(const VrEvent& e) {
        std::lock_guard<std::mutex> lk(eventMutex);
        events.push_back(e);
        if (events.size() > 64) events.erase(events.begin());
    }
};

VrEngineImpl* gEngine = nullptr;

} // namespace

// ===========================================================================
// Tracking
// ===========================================================================
void Tracking::pushImu(const Vec3& gyroRadSec, const Vec3& accelMs2,
                       const Vec3* mag, int64_t timestampNs) {
    if (!gEngine) return;
    float dt = lastImuNs_ ? nsToSecondsF(timestampNs - lastImuNs_) : 1.0f / 240.0f;
    lastImuNs_ = timestampNs;
    if (dt <= 0.0f || dt > 0.5f) return;

    std::lock_guard<std::mutex> lk(gEngine->poseMutex);
    Quat q = imu_.update(gyroRadSec, accelMs2, mag, dt);
    lastGyro_ = gyroRadSec;

    // integra aceleração p/ deslocamento (escala do VO monocular)
    // remove gravidade usando a orientação estimada
    // mundo Y-up: gravidade = (0,-9.81,0); no corpo: q⁻¹·(0,-9.81,0)
    Vec3 gravityBody = q.conjugate().rotate(Vec3{0, -9.81f, 0});
    Vec3 linAccel = accelMs2 + gravityBody; // ≈ aceleração linear no corpo
    Vec3 linWorld = q.rotate(linAccel);
    velocity_ = velocity_ + linWorld * dt;
    // decaimento forte: acelerômetro sozinho deriva; serve só de pista de escala
    velocity_ = velocity_ * (1.0f - clampf(dt * 2.0f, 0.0f, 0.9f));
    Vec3 disp = velocity_ * dt;
    if (backend_ == TrackingBackend::CV_SLAM) vo_.feedImuDisplacement(disp);

    rawPose_.orientation = q;
    rawPose_.angularVelocity = gyroRadSec;
    rawPose_.position = {0, 0, 0};
    rawPose_.dofs = 3;
    rawPose_.confidence = 0.9f;
    rawPose_.timestampNs = timestampNs;
    externalPoseValid_ = false;
}

void Tracking::setPoseFromExternal(const Vec3& pos, const Quat& q,
                                   int64_t timestampNs, float confidence) {
    if (!gEngine) return;
    std::lock_guard<std::mutex> lk(gEngine->poseMutex);
    rawPose_.position = pos;
    rawPose_.orientation = q;
    rawPose_.timestampNs = timestampNs;
    rawPose_.confidence = confidence;
    rawPose_.dofs = 6;
    externalPoseValid_ = true;
    // ARCore traz velocidade? estimada por diferença (para a predição)
    static Vec3 lastPos = pos;
    static int64_t lastTs = timestampNs;
    float dt = nsToSecondsF(timestampNs - lastTs);
    if (dt > 0.001f && dt < 0.5f) {
        rawPose_.velocity = (pos - lastPos) / dt;
    }
    lastPos = pos;
    lastTs = timestampNs;
}

void Tracking::pushCameraFrame(const uint8_t* luma, int w, int h, int stride,
                               int64_t timestampNs) {
    if (!gEngine || backend_ != TrackingBackend::CV_SLAM) return;
    // limita a ~30 Hz de VO: câmera pode rodar a 30/60
    if (lastCamNs_ && nsToSecondsF(timestampNs - lastCamNs_) < 1.0f / 31.0f)
        return;
    lastCamNs_ = timestampNs;

    LumaView view{reinterpret_cast<const uint8_t*>(luma), w, h, stride};
    (void)view; // VO roda no update() com o último frame armazenado
    // (cópia controlada — o buffer Java é reciclado)
    frameCopy_.assign(luma, luma + static_cast<std::size_t>(stride) * h);
    frameW_ = w;
    frameH_ = h;
    frameStride_ = stride;
    frameTs_ = timestampNs;
}

void Tracking::pushImuDisplacement(const Vec3& displacementM) {
    if (backend_ == TrackingBackend::CV_SLAM) vo_.feedImuDisplacement(displacementM);
}

void Tracking::update(int64_t nowNs) {
    if (!gEngine) return;
    std::lock_guard<std::mutex> lk(gEngine->poseMutex);

    // backend CV: processa frame pendente e integra posição
    if (backend_ == TrackingBackend::CV_SLAM && !frameCopy_.empty()) {
        LumaView view{frameCopy_.data(), frameW_, frameH_, frameStride_};
        VoResult res = vo_.processFrame(view, frameTs_);
        if (res.ok) {
            rawPose_.position = vo_.position();
            rawPose_.orientation = imu_.orientation();
            rawPose_.dofs = 6;
            rawPose_.confidence =
                res.inliers > 30 ? 0.8f : 0.45f;
            rawPose_.timestampNs = frameTs_;
        } else {
            rawPose_.orientation = imu_.orientation();
            rawPose_.position = vo_.position();
            rawPose_.dofs = 6; // posição mantida por inércia
            rawPose_.confidence = 0.4f;
        }
        frameCopy_.clear();
    }

    Pose raw = rawPose_;
    raw.timestampNs = nowNs;
    filteredPose_ = pipeline_.process(raw);
}

// ===========================================================================
// Hands
// ===========================================================================
void Hands::setLandmarks(bool left, const Vec3* landmarks21, float confidence,
                         int gesture, float pinchStrength, int64_t timestampNs) {
    if (!gEngine || !landmarks21) return;
    int idx = left ? 0 : 1;
    for (int i = 0; i < kNumHandLandmarks; ++i)
        camLandmarks_[idx][i] = landmarks21[i];
    lastUpdateNs_[idx] = timestampNs;

    HandState& h = left ? left_ : right_;
    h.present = confidence > 0.35f;
    h.left = left;
    h.confidence = confidence;
    h.gesture = static_cast<HandGesture>(gesture);
    h.pinchStrength = pinchStrength;
    // pinch point no espaço da câmera (atualizado no RenderFrame p/ mundo)
    h.pinchPoint = (camLandmarks_[idx][4] + camLandmarks_[idx][8]) * 0.5f;
    h.indexTip = camLandmarks_[idx][8];
    h.palmCenter = (camLandmarks_[idx][0] + camLandmarks_[idx][9]) * 0.5f;
}

// ===========================================================================
// Windows
// ===========================================================================
int Windows::create(int requestedId, const Vec3& pos, const Quat& rot,
                    float widthM, float heightM, WindowContent content) {
    if (static_cast<int>(windows_.size()) >= limits::kMaxSpatialWindows) {
        // remove a mais antiga não focada
        for (auto it = windows_.begin(); it != windows_.end(); ++it) {
            if (it->id != focusedId_) { windows_.erase(it); break; }
        }
    }
    SpatialWindow w;
    w.id = requestedId > 0 ? requestedId : static_cast<int>(windows_.size() + 1);
    // garante id único
    while (true) {
        bool dup = false;
        for (auto& e : windows_) if (e.id == w.id) dup = true;
        if (!dup) break;
        ++w.id;
    }
    w.position = pos;
    w.orientation = rot;
    w.widthM = widthM;
    w.heightM = heightM;
    w.content = content;
    w.birthTime = 0.001f;
    windows_.push_back(w);
    return w.id;
}

bool Windows::remove(int windowId) {
    for (auto it = windows_.begin(); it != windows_.end(); ++it) {
        if (it->id == windowId) {
            windows_.erase(it);
            if (focusedId_ == windowId) focusedId_ = -1;
            return true;
        }
    }
    return false;
}

SpatialWindow* Windows::find(int windowId) {
    for (auto& w : windows_)
        if (w.id == windowId) return &w;
    return nullptr;
}

void Windows::focus(int windowId) {
    focusedId_ = windowId;
    for (auto& w : windows_) w.focused = (w.id == windowId);
}

// ===========================================================================
// Scene + mini física
// ===========================================================================
uint64_t Scene::addModel(std::shared_ptr<RuntimeModel> model, const Vec3& pos) {
    if (!model) return 0;
    SpawnedObject o;
    o.id = nextId_++;
    o.kind = SpawnedObject::Kind::MODEL;
    o.model = std::move(model);
    o.position = pos;
    o.gravity = false;
    o.radius = 0.25f;
    objects_.push_back(o);
    return o.id;
}

uint64_t Scene::spawnSphere(const Vec3& pos, float radius) {
    SpawnedObject o;
    o.id = nextId_++;
    o.kind = SpawnedObject::Kind::SPHERE;
    o.position = pos;
    o.radius = radius;
    objects_.push_back(o);
    return o.id;
}

uint64_t Scene::spawnCube(const Vec3& pos, float halfExtent) {
    SpawnedObject o;
    o.id = nextId_++;
    o.kind = SpawnedObject::Kind::CUBE;
    o.position = pos;
    o.radius = halfExtent;
    objects_.push_back(o);
    return o.id;
}

bool Scene::removeObject(uint64_t id) {
    for (auto it = objects_.begin(); it != objects_.end(); ++it) {
        if (it->id == id) { objects_.erase(it); return true; }
    }
    return false;
}

SpawnedObject* Scene::find(uint64_t id) {
    for (auto& o : objects_)
        if (o.id == id) return &o;
    return nullptr;
}

void Scene::update(float dtSec) {
    for (auto& o : objects_) {
        if (o.model && o.animIndex >= 0) {
            o.model->setActiveAnimation(o.animIndex, o.animLoop);
            o.animTime += dtSec;
        }
    }
    for (auto& o : objects_)
        if (o.model) o.model->update(dtSec);
    if (physicsEnabled_) {
        physicsAccum_ += dtSec;
        const float step = 1.0f / 60.0f;
        int guard = 0;
        while (physicsAccum_ >= step && guard++ < 4) {
            stepPhysics(step);
            physicsAccum_ -= step;
        }
    } else {
        physicsAccum_ = 0.0f;
    }
}

void Scene::stepPhysics(float dt) {
    const float kGround = 0.0f;
    const float kRestitution = 0.45f;
    const float kDamping = 0.995f;

    for (auto& o : objects_) {
        if (!o.gravity) continue;
        o.velocity.y -= 9.81f * dt;
        o.position = o.position + o.velocity * dt;
        // chão
        if (o.position.y - o.radius < kGround) {
            o.position.y = kGround + o.radius;
            if (o.velocity.y < 0) o.velocity.y = -o.velocity.y * kRestitution;
            o.velocity.x *= 0.96f;
            o.velocity.z *= 0.96f;
        }
        // limites da sala virtual (raio 6 m)
        float r = std::sqrt(o.position.x * o.position.x + o.position.z * o.position.z);
        if (r > 6.0f) {
            float s = 6.0f / r;
            o.position.x *= s;
            o.position.z *= s;
            o.velocity.x *= -kRestitution;
            o.velocity.z *= -kRestitution;
        }
        o.velocity = o.velocity * kDamping;
        o.orientation = Quat::fromAxisAngle(o.angularVel,
                                            o.angularVel.length() * dt) *
                        o.orientation;
    }
    // colisão esfera-esfera (n² pequeno: objetos são poucos)
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        for (std::size_t j = i + 1; j < objects_.size(); ++j) {
            SpawnedObject& a = objects_[i];
            SpawnedObject& b = objects_[j];
            if (!a.gravity && !b.gravity) continue;
            Vec3 d = b.position - a.position;
            float dist = d.length();
            float minDist = a.radius + b.radius;
            if (dist < minDist && dist > 1e-5f) {
                Vec3 n = d / dist;
                float push = (minDist - dist) * 0.5f;
                a.position = a.position - n * push;
                b.position = b.position + n * push;
                float relV = (b.velocity - a.velocity).dot(n);
                if (relV < 0) {
                    float imp = -relV * 0.8f;
                    a.velocity = a.velocity - n * imp;
                    b.velocity = b.velocity + n * imp;
                }
            }
        }
    }
}

// ===========================================================================
// API global
// ===========================================================================
bool Initialize(const VrInitParams& params) {
    if (gEngine) return true;
    gEngine = new VrEngineImpl();
    gEngine->params = params;
    gEngine->eventCallback = params.eventCallback;
    gEngine->headset.profile() = StereoProfile{};
    gEngine->tracking.filterConfig() = PoseFilterConfig{};
    gEngine->tracking.applyFilterConfig();
    BMR_LOGI("VR::Initialize ok (tela %dx%d)", params.screenW, params.screenH);
    return true;
}

void Shutdown() {
    if (!gEngine) return;
    if (gEngine->glReady) {
        gEngine->compositor.destroy();
        gEngine->stereo.destroy();
        gEngine->sky.destroy();
        gEngine->models.destroy();
        gEngine->windowRenderer.destroy();
        gEngine->handRenderer.destroy();
        gEngine->uiRenderer.destroy();
    }
    delete gEngine;
    gEngine = nullptr;
    BMR_LOGI("VR::Shutdown ok");
}

bool IsInitialized() { return gEngine != nullptr; }

Headset& GetHeadset() { return gEngine->headset; }
Tracking& GetTracking() { return gEngine->tracking; }
Hands& GetHands() { return gEngine->hands; }
Windows& GetWindows() { return gEngine->windows; }
Scene& GetScene() { return gEngine->scene; }
UiScene& GetUiScene() { return gEngine->ui; }
Compositor& GetCompositor() { return gEngine->compositor; }

std::shared_ptr<RuntimeModel> LoadGLB(const uint8_t* data, std::size_t size,
                                      std::string& err) {
    GltfModel gltf;
    if (!parseGlb(data, size, gltf)) {
        err = gltf.lastError.empty() ? "GLB inválido" : gltf.lastError;
        return nullptr;
    }
    auto model = RuntimeModel::fromGltf(gltf, err);
    if (model && gEngine && gEngine->glReady) {
        gEngine->models.registerModel(model);
    }
    return model;
}

GLuint UploadModelTexture(const std::shared_ptr<RuntimeModel>& model,
                          int imageIndex, int w, int h, const uint8_t* rgba) {
    if (!gEngine || !gEngine->glReady) return 0;
    // textura própria e transferida para o cache do renderer
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if (!tex) return 0;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGenerateMipmap(GL_TEXTURE_2D);
    gEngine->models.setModelTexture(model, imageIndex, tex);
    return tex;
}

void PushPointer(PointerSource source, const Vec3& origin, const Vec3& dir,
                 int pointerEvent) {
    if (!gEngine) return;
    VrEngineImpl& e = *gEngine;

    // 1) janelas espaciais (apps/browser)
    WindowHit hit = e.windows.raycast(origin, dir);
    if (hit.hit) {
        if (pointerEvent == 1) { // press
            e.windows.focus(hit.windowId);
            VrEvent ev;
            ev.type = VrEventType::UI_PANEL_FOCUS;
            ev.a = hit.windowId;
            e.queueEvent(ev);
        }
        VrEvent ev;
        ev.type = VrEventType::WINDOW_INPUT;
        ev.a = hit.windowId;
        ev.x = hit.u;
        ev.y = hit.v;
        ev.z = static_cast<float>(pointerEvent);
        e.queueEvent(ev);
        return;
    }

    // 2) UI espacial
    std::vector<UiEvent> uiEvents;
    UiPointerEvent pe = static_cast<UiPointerEvent>(pointerEvent);
    e.ui.dispatchPointer(origin, dir, pe, uiEvents);
    for (auto& ue : uiEvents) {
        VrEvent ev;
        switch (ue.type) {
            case UI_EVENT_BUTTON:    ev.type = VrEventType::UI_BUTTON; break;
            case UI_EVENT_SLIDER:    ev.type = VrEventType::UI_SLIDER; break;
            case UI_EVENT_TOGGLE:    ev.type = VrEventType::UI_TOGGLE; break;
            case UI_EVENT_HOVER:     ev.type = VrEventType::UI_HOVER; break;
            case UI_EVENT_PANEL_FOCUS: ev.type = VrEventType::UI_PANEL_FOCUS; break;
            default: ev.type = VrEventType::NONE; break;
        }
        ev.a = ue.panelId;
        ev.b = ue.controlId;
        ev.x = ue.value;
        e.queueEvent(ev);
    }

    // 3) clique no vazio → evento de ponteiro (Lua/apps podem usar)
    if (pointerEvent == 1) {
        Vec3 p = origin + dir.normalized() * 3.0f;
        VrEvent ev;
        ev.type = VrEventType::POINTER_CLICK;
        ev.x = p.x; ev.y = p.y; ev.z = p.z;
        ev.a = static_cast<int>(source);
        e.queueEvent(ev);
    }
}

void SetActiveRay(bool visible, const Vec3& origin, const Vec3& dir,
                  const Vec3& color) {
    if (!gEngine) return;
    std::lock_guard<std::mutex> lk(gEngine->rayMutex);
    gEngine->rayVisible = visible;
    gEngine->rayOrigin = origin;
    gEngine->rayDir = dir.normalized();
    gEngine->rayColor = color;
}

bool RenderFrame(float dtSec, int64_t nowNs) {
    if (!gEngine) return false;
    VrEngineImpl& e = *gEngine;
    if (!e.glReady) return false;

    int64_t t0 = ::brazilmr::nowNs();

    // 1) tracking: filtra/prediz pose
    e.tracking.update(nowNs);
    Pose head;
    {
        std::lock_guard<std::mutex> lk(e.poseMutex);
        head = e.tracking.filteredPose_;
        e.qAtSceneRender = head.orientation;
    }

    // recenter (do painel/tecla)
    if (e.headset.consumeRecenter()) {
        std::lock_guard<std::mutex> lk(e.poseMutex);
        float yaw, pitch, roll;
        e.tracking.filteredPose_.orientation.toEulerYXZ(yaw, pitch, roll);
        e.tracking.filteredPose_.orientation = Quat::fromEulerYXZ(0, pitch, roll);
        e.tracking.imu_.recenterYaw();
        e.tracking.pipeline_.reset();
        head = e.tracking.filteredPose_;
    }

    // 2) cena + física + animações
    e.scene.update(dtSec);

    // 3) mãos → espaço do mundo (câmera = pose da cabeça)
    {
        const Quat& q = head.orientation;
        const Vec3& p = head.position;
        for (int hIdx = 0; hIdx < 2; ++hIdx) {
            HandState& hs = hIdx == 0 ? e.hands.left_ : e.hands.right_;
            if (!hs.present) continue;
            const Vec3* cam = e.hands.camLandmarks_[hIdx];
            for (int i = 0; i < kNumHandLandmarks; ++i)
                hs.landmarks[i] = p + q.rotate(cam[i]);
            hs.pinchPoint = p + q.rotate(e.hands.camLandmarks_[hIdx][4] * 0.5f +
                                         e.hands.camLandmarks_[hIdx][8] * 0.5f);
            hs.indexTip = p + q.rotate(e.hands.camLandmarks_[hIdx][8]);
            hs.palmCenter = p + q.rotate((e.hands.camLandmarks_[hIdx][0] +
                                          e.hands.camLandmarks_[hIdx][9]) * 0.5f);
        }
        e.handRenderer.update(e.hands.left_, e.hands.right_, dtSec,
                              nsToSeconds(nowNs));
    }

    // 4) render por olho
    e.drawCalls = 0;
    SceneLight light;
    float skyDetail = e.fx.skyDetail;

    for (int eye = 0; eye < 2; ++eye) {
        e.stereo.renderEye(eye, head.position, head.orientation,
                           [&](const Camera& cam) {
            Mat4 invVp = cam.viewProj.inverted();
            e.sky.draw(invVp, skyDetail, nsToSeconds(nowNs));
            ++e.drawCalls;

            // modelos GLB da cena
            for (auto& o : e.scene.objects()) {
                if (!o.visible) continue;
                if (o.model) {
                    o.model->position = o.position;
                    o.model->orientation = o.orientation;
                    o.model->scale = o.scale;
                    e.models.draw(o.model, cam, light);
                    ++e.drawCalls;
                } else {
                    // primitivas: desenhadas como janela-quad colorida simples
                    // (esfera/cubo estilizados via quad billboard + cor)
                    SpatialWindow prim;
                    prim.position = o.position;
                    prim.orientation = cam.orientation; // billboard
                    prim.widthM = o.radius * 2.2f;
                    prim.heightM = o.radius * 2.2f;
                    prim.content = WindowContent::LUA_UI;
                    e.windowRenderer.drawPlaceholder(prim, cam,
                                                     nsToSeconds(nowNs));
                    ++e.drawCalls;
                }
            }

            // janelas espaciais
            for (auto& w : e.windows.all()) {
                if (!w.visible) continue;
                e.windowRenderer.draw(w, cam, nsToSeconds(nowNs), e.fx);
                ++e.drawCalls;
            }

            // UI espacial
            e.uiRenderer.drawScene(e.ui, cam, e.fx, nsToSeconds(nowNs));
            e.drawCalls += 3;

            // mãos
            e.handRenderer.draw(cam, e.fx);
            e.drawCalls += 8;

            // raio do ponteiro (gaze/controller)
            {
                std::lock_guard<std::mutex> lk(e.rayMutex);
                if (e.rayVisible) {
                    e.windowRenderer.drawRay(e.rayOrigin, e.rayDir, 5.0f,
                                             e.rayColor, cam);
                    ++e.drawCalls;
                }
            }
        });
    }

    // 5) late-latch: correção de rotação entre o render da cena e AGORA
    float reproj[4] = {1, 0, 0, 1};
    {
        std::lock_guard<std::mutex> lk(e.poseMutex);
        // usa a última orientação recebida (sensor thread atualiza em paralelo)
        Quat qLatest = e.tracking.filteredPose_.orientation;
        Quat qDelta = qLatest * e.qAtSceneRender.conjugate();
        // extrai yaw/pitch (ignora roll — artefato de warp)
        float yaw, pitch, roll;
        qDelta.toEulerYXZ(yaw, pitch, roll);
        (void)roll;
        // pequenas rotações → matriz 2x2 no plano da tela (tan-space)
        // (aproximação de pequeno ângulo: yaw gira o eixo X, pitch o Y)
        float cy = std::cos(-yaw), sy = std::sin(-yaw);
        float cp = std::cos(pitch);
        reproj[0] = cy;
        reproj[1] = sy;
        reproj[2] = -sy * cp;
        reproj[3] = cy * cp;
        // limita a magnitude (não reprojeta giros grandes)
        float maxOff = 0.06f;
        reproj[1] = clampf(reproj[1], -maxOff, maxOff);
        reproj[2] = clampf(reproj[2], -maxOff, maxOff);
    }

    // 6) compositor final
    e.compositor.draw(e.stereo.eyeFbo(0).colorTexture(),
                      e.stereo.eyeFbo(1).colorTexture(), reproj,
                      nsToSeconds(nowNs));

    // 7) stats
    float frameMs = static_cast<float>(::brazilmr::nowNs() - t0) * 1e-6f;
    e.frameMsEma = lerpf(e.frameMsEma, frameMs, 0.1f);
    e.fpsEst = 1000.0f / std::fmax(e.frameMsEma, 1.0f);
    return true;
}

bool PollEvent(VrEvent& out) {
    if (!gEngine) return false;
    std::lock_guard<std::mutex> lk(gEngine->eventMutex);
    if (gEngine->events.empty()) return false;
    out = gEngine->events.front();
    gEngine->events.erase(gEngine->events.begin());
    return true;
}

void SetThermalBudget(float budget01) {
    if (!gEngine) return;
    budget01 = clampf(budget01, 0.0f, 1.0f);
    gEngine->thermalBudget = budget01;
    // escada de degradação de efeitos
    gEngine->fx.handGlow = clampf(budget01 * 1.2f, 0.2f, 1.0f);
    gEngine->fx.handParticles = budget01 > 0.7f ? 1.0f : (budget01 > 0.4f ? 0.5f : 0.0f);
    gEngine->fx.windowFxBudget = clampf(budget01, 0.15f, 1.0f);
    gEngine->fx.skyDetail = budget01 > 0.5f ? 1.0f : 0.4f;
}

float ThermalBudget() { return gEngine ? gEngine->thermalBudget : 1.0f; }

void GetStats(float* out7) {
    if (!gEngine || !out7) return;
    VrEngineImpl& e = *gEngine;
    out7[0] = e.fpsEst;
    out7[1] = e.frameMsEma;
    out7[2] = static_cast<float>(e.stereo.eyeTargetWidth(0));
    out7[3] = static_cast<float>(e.stereo.eyeTargetHeight(0));
    out7[4] = static_cast<float>(e.drawCalls);
    out7[5] = static_cast<float>(e.scene.objects().size());
    out7[6] = static_cast<float>(e.windows.all().size());
}

// ===========================================================================
// GL lifecycle (chamado pelo JNI no thread GL)
// ===========================================================================
void ResizeGl(int screenW, int screenH); // forward

bool InitGl(int screenW, int screenH) {
    if (!gEngine) return false;
    VrEngineImpl& e = *gEngine;
    if (e.glReady) {
        ResizeGl(screenW, screenH);
        return true;
    }
    bool ok = e.compositor.init() && e.stereo.init() && e.sky.init() &&
              e.models.init() && e.windowRenderer.init() &&
              e.handRenderer.init() && e.uiRenderer.init();
    if (!ok) {
        BMR_LOGE("falha ao inicializar renderers GL");
        return false;
    }
    e.compositor.configure(e.headset.profile(), screenW, screenH);
    e.stereo.configure(e.headset.profile(), screenW, screenH);
    e.glReady = true;
    BMR_LOGI("GL pronto: %dx%d, alvos de olho %dx%d", screenW, screenH,
             e.stereo.eyeTargetWidth(0), e.stereo.eyeTargetHeight(0));
    return true;
}

void ResizeGl(int screenW, int screenH) {
    if (!gEngine) return;
    VrEngineImpl& e = *gEngine;
    e.compositor.configure(e.headset.profile(), screenW, screenH);
    e.stereo.configure(e.headset.profile(), screenW, screenH);
}

void DestroyGl() {
    if (!gEngine) return;
    VrEngineImpl& e = *gEngine;
    if (!e.glReady) return;
    e.compositor.destroy();
    e.stereo.destroy();
    e.sky.destroy();
    e.models.destroy();
    e.windowRenderer.destroy();
    e.handRenderer.destroy();
    e.uiRenderer.destroy();
    e.glReady = false;
}

void SetFontAtlas(GLuint texId, int atlasW, int atlasH, float baseHeightPx,
                  const std::vector<GlyphInfo>& glyphs) {
    if (!gEngine) return;
    gEngine->uiRenderer.textAtlas().set(texId, atlasW, atlasH, baseHeightPx,
                                        glyphs);
}

float TsToSeconds(int64_t ns) { return nsToSeconds(ns); }

} // namespace VR
} // namespace brazilmr
