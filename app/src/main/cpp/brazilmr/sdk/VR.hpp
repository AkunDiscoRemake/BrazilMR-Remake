// ===========================================================================
// BrazilMR VR SDK — API C++ pública.
//
//   VR::Initialize(params);
//   auto headset  = VR::GetHeadset();
//   auto tracking = VR::GetTracking();
//   auto hands    = VR::GetHands();
//   auto model    = VR::LoadGLB(bytes, size);
//   model->SetPosition(0, 1, -2);
//   ...
//   VR::RenderFrame(dt);
//   VR::Shutdown();
//
// O pipeline por frame:
//   SENSORES/CÂMERA (Kotlin/Java) → Tracking (nativo) → FILTROS (One Euro +
//   Kalman + predição) → RENDER ESTÉREO POR OLHO (SBS) → COMPOSITOR (warp de
//   lente + cromática + vignette + late-latch) → TELA.
// ===========================================================================
#pragma once

#include "../BrazilmrConfig.hpp"
#include "../filters/PoseFilterPipeline.hpp"
#include "../gl/GlUtils.hpp"
#include "../glb/GltfRuntime.hpp"
#include "../hands/HandTypes.hpp"
#include "../renderer/Camera.hpp"
#include "../renderer/Compositor.hpp"
#include "../renderer/HandRenderer.hpp"
#include "../renderer/ModelRenderer.hpp"
#include "../renderer/SkyRenderer.hpp"
#include "../renderer/StereoRenderer.hpp"
#include "../renderer/UiRenderer.hpp"
#include "../renderer/WindowRenderer.hpp"
#include "../tracking/ImuFusion.hpp"
#include "../tracking/TrackingTypes.hpp"
#include "../tracking/VisualOdometry.hpp"
#include "../ui/UiScene.hpp"
#include "../windows/SpatialWindow.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace brazilmr {
namespace VR {

// ---------------------------------------------------------------------------
// Eventos que sobem para a camada Java (JNI) e daí para Lua/apps.
// ---------------------------------------------------------------------------
enum class VrEventType : int {
    NONE = 0,
    UI_BUTTON       = 1,   // a=panelId, b=controlId
    UI_SLIDER       = 2,   // a=panelId, b=controlId, x=value
    UI_TOGGLE       = 3,   // a=panelId, b=controlId, x=value
    UI_HOVER        = 4,   // a=panelId, b=controlId
    UI_PANEL_FOCUS  = 5,   // a=panelId
    WINDOW_INPUT    = 6,   // a=windowId, x=u, y=v, z=pointerEvent
    GESTURE         = 7,   // a=hand(0/1), b=gesture, x=strength
    POINTER_CLICK   = 8,   // clique de controller/gaze: x,y,z = ponto 3D
};

struct VrEvent {
    VrEventType type = VrEventType::NONE;
    int a = 0, b = 0;
    float x = 0, y = 0, z = 0;
    std::string s;
};

// ---------------------------------------------------------------------------
// Headset — perfil estéreo/calibração.
// ---------------------------------------------------------------------------
class Headset {
public:
    StereoProfile& profile() { return profile_; }
    const StereoProfile& profile() const { return profile_; }
    void recenter() { recenterRequested_ = true; }
    bool consumeRecenter() {
        bool r = recenterRequested_;
        recenterRequested_ = false;
        return r;
    }
private:
    StereoProfile profile_;
    bool recenterRequested_ = false;
};

// ---------------------------------------------------------------------------
// Tracking — entrada de sensores, backends, filtragem e pose final.
// ---------------------------------------------------------------------------
class Tracking {
public:
    // IMU direto (eixos já remapeados para "cabeça" pela camada Java).
    void pushImu(const Vec3& gyroRadSec, const Vec3& accelMs2,
                 const Vec3* mag, int64_t timestampNs);

    // Pose vinda do ARCore (backend externo, autoritativo quando ativo).
    void setPoseFromExternal(const Vec3& pos, const Quat& q,
                             int64_t timestampNs, float confidence);

    // Frame de câmera (luminância) para o backend CV.
    void pushCameraFrame(const uint8_t* luma, int w, int h, int stride,
                         int64_t timestampNs);

    // Deslocamento do IMU (m) para escala métrica do VO monocular.
    void pushImuDisplacement(const Vec3& displacementM);

    void setBackend(TrackingBackend b) { backend_ = b; }
    TrackingBackend backend() const { return backend_; }
    const char* backendName() const { return trackingBackendName(backend_); }

    // Pose da cabeça FILTRADA (One Euro → Kalman → predição).
    const Pose& headPose() const { return filteredPose_; }
    const Pose& rawPose() const { return rawPose_; }

    // Configuração dos filtros (painel de calibração).
    PoseFilterConfig& filterConfig() { return filterCfg_; }
    void applyFilterConfig() { pipeline_.configure(filterCfg_); }

    void update(int64_t nowNs);

    // Estatísticas para o painel de performance.
    int lastVoFeatures() const { return vo_.lastFeatureCount(); }
    float voScale() const { return vo_.scale(); }

    void reset() {
        imu_.reset();
        vo_.reset();
        pipeline_.reset();
        rawPose_ = Pose();
        filteredPose_ = Pose();
    }

    // ---- estado interno (acessado pelo engine; documentado como interno) ----
    TrackingBackend backend_ = TrackingBackend::IMU_3DOF;
    ImuFusion imu_;
    VisualOdometry vo_;
    PoseFilterPipeline pipeline_;
    PoseFilterConfig filterCfg_;
    Pose rawPose_;
    Pose filteredPose_;
    Vec3 lastGyro_{0, 0, 0};
    Vec3 velocity_{0, 0, 0};
    int64_t lastImuNs_ = 0;
    int64_t lastCamNs_ = 0;
    bool externalPoseValid_ = false;

    // frame de câmera pendente (copiado do buffer Java; reciclado no update)
    std::vector<uint8_t> frameCopy_;
    int frameW_ = 0, frameH_ = 0, frameStride_ = 0;
    int64_t frameTs_ = 0;

    static float nsToSecondsF(int64_t ns) {
        return static_cast<float>(ns) * 1e-9f;
    }
};

// ---------------------------------------------------------------------------
// Hands — estado das mãos (landmarks → mundo) + visual.
// ---------------------------------------------------------------------------
class Hands {
public:
    // Landmarks em espaço da CÂMERA (x direita, y cima, -z à frente), metros.
    void setLandmarks(bool left, const Vec3* landmarks21, float confidence,
                      int gesture, float pinchStrength, int64_t timestampNs);
    const HandState& left() const { return left_; }
    const HandState& right() const { return right_; }

    // ---- estado interno (engine) ----
    HandState left_, right_;
    Vec3 camLandmarks_[2][kNumHandLandmarks];
    int64_t lastUpdateNs_[2] = {0, 0};
};

// ---------------------------------------------------------------------------
// Windows — janelas espaciais.
// ---------------------------------------------------------------------------
class Windows {
public:
    int create(int requestedId, const Vec3& pos, const Quat& rot, float widthM,
               float heightM, WindowContent content);
    bool remove(int windowId);
    SpatialWindow* find(int windowId);
    std::vector<SpatialWindow>& all() { return windows_; }
    WindowHit raycast(const Vec3& origin, const Vec3& dir) const {
        return raycastWindows(origin, dir, windows_);
    }
    void focus(int windowId);
    int focused() const { return focusedId_; }

private:
    std::vector<SpatialWindow> windows_;
    int focusedId_ = -1;
};

// ---------------------------------------------------------------------------
// Scene — modelos GLB + primitivas + mini física (para a SDK Lua/C++).
// ---------------------------------------------------------------------------
struct SpawnedObject {
    uint64_t id = 0;
    enum class Kind { MODEL, SPHERE, CUBE } kind = Kind::SPHERE;
    std::shared_ptr<RuntimeModel> model;
    Vec3 position{0, 0, -2};
    Quat orientation = Quat::identity();
    Vec3 scale{1, 1, 1};
    Vec3 velocity{0, 0, 0};
    Vec3 angularVel{0, 0, 0};
    float color[4] = {0.2f, 0.8f, 0.6f, 1.0f};
    bool gravity = true;
    bool visible = true;
    float radius = 0.08f;   // colisão
    int animIndex = -1;
    bool animLoop = true;
    float animTime = 0.0f;
};

class Scene {
public:
    uint64_t addModel(std::shared_ptr<RuntimeModel> model, const Vec3& pos);
    uint64_t spawnSphere(const Vec3& pos, float radius);
    uint64_t spawnCube(const Vec3& pos, float halfExtent);
    bool removeObject(uint64_t id);
    SpawnedObject* find(uint64_t id);
    std::vector<SpawnedObject>& objects() { return objects_; }

    void setPhysicsEnabled(bool on) { physicsEnabled_ = on; }
    void update(float dtSec);

private:
    std::vector<SpawnedObject> objects_;
    uint64_t nextId_ = 1;
    bool physicsEnabled_ = true;
    float physicsAccum_ = 0.0f;
    void stepPhysics(float dt);
};

// ---------------------------------------------------------------------------
// API global
// ---------------------------------------------------------------------------
struct VrInitParams {
    int screenW = 1280;
    int screenH = 720;
    // Callback de eventos (chamado no thread GL; veja docs de threading).
    // void (*)(const VrEvent&) — nullptr = sem eventos
    void (*eventCallback)(const VrEvent&) = nullptr;
};

bool Initialize(const VrInitParams& params);
void Shutdown();
bool IsInitialized();

Headset&  GetHeadset();
Tracking& GetTracking();
Hands&    GetHands();
Windows&  GetWindows();
Scene&    GetScene();
UiScene&  GetUiScene();
Compositor& GetCompositor();

// Carrega um modelo GLB de bytes (arquivo lido pela camada superior).
// Retorna nullptr + mensagem de erro em err.
std::shared_ptr<RuntimeModel> LoadGLB(const uint8_t* data, std::size_t size,
                                      std::string& err);

// Upload de textura de modelo (RGBA8888). Devolve o id da textura GL.
GLuint UploadModelTexture(const std::shared_ptr<RuntimeModel>& model,
                          int imageIndex, int w, int h, const uint8_t* rgba);

// Ponteiro espacial (gaze/controller/mão) — roteia para janelas e UI.
enum class PointerSource : int { GAZE = 0, HAND = 1, CONTROLLER = 2 };
void PushPointer(PointerSource source, const Vec3& origin, const Vec3& dir,
                 int pointerEvent); // 0=hover,1=press,2=release,3=move
// Estado do raio ativo para desenho (setado pela camada de input Java).
void SetActiveRay(bool visible, const Vec3& origin, const Vec3& dir,
                  const Vec3& color);

// Frame completo: render dos dois olhos + compositor.
// headPoseOptional: se nullptr, usa o tracking interno.
bool RenderFrame(float dtSec, int64_t nowNs);

// Drena eventos para o JNI/Lua.
bool PollEvent(VrEvent& out);

// Performance: nível térmico 0..1 (0 = folga, 1 = limite). Escala efeitos.
void SetThermalBudget(float budget01);
float ThermalBudget();

// Estatísticas [fpsEst, frameMs, eyesW, eyesH, drawCalls, models, windows]
void GetStats(float* out7);

// ---- ciclo de vida GL (chamados pelo JNI no thread GL) ----
bool InitGl(int screenW, int screenH);
void ResizeGl(int screenW, int screenH);
void DestroyGl();

// Atlas de fonte (gerado em Kotlin, upload nativo).
void SetFontAtlas(GLuint texId, int atlasW, int atlasH, float baseHeightPx,
                  const std::vector<GlyphInfo>& glyphs);

} // namespace VR
} // namespace brazilmr
