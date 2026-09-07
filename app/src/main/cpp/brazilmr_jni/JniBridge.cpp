// ===========================================================================
// BrazilMR — ponte JNI.
// Toda a superfície nativa do runtime é exposta aqui para o Kotlin
// (com.brazilmr.sdk.NativeSdk). Thread GL: funções de render/upload.
// Threads de sensores/câmera: push* (protegidos por mutex no core).
// ===========================================================================
#include <jni.h>
#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#endif
#include <cstring>
#include <string>
#include <vector>

#include "brazilmr/core/Log.hpp"
#include "brazilmr/sdk/VR.hpp"

using namespace brazilmr;
using namespace brazilmr::VR;

namespace {

// helper: jstring → std::string
std::string toStr(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* c = env->GetStringUTFChars(s, nullptr);
    std::string out(c ? c : "");
    env->ReleaseStringUTFChars(s, c);
    return out;
}

inline Vec3 vec3(jfloat x, jfloat y, jfloat z) { return Vec3{x, y, z}; }
inline Quat quat(jfloat x, jfloat y, jfloat z, jfloat w) { return Quat{x, y, z, w}; }

} // namespace

extern "C" {

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeInitialize(JNIEnv*, jobject,
                                                 jint screenW, jint screenH) {
    VrInitParams p;
    p.screenW = screenW;
    p.screenH = screenH;
    return Initialize(p) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeShutdown(JNIEnv*, jobject) {
    Shutdown();
}

JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeInitGl(JNIEnv*, jobject, jint w, jint h) {
    return InitGl(w, h) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeResizeGl(JNIEnv*, jobject, jint w, jint h) {
    ResizeGl(w, h);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeDestroyGl(JNIEnv*, jobject) {
    DestroyGl();
}

// ---------------------------------------------------------------------------
// perfil estéreo / calibração
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetStereoProfile(
    JNIEnv*, jobject, jfloat fovYDeg, jfloat ipdMm, jfloat k1, jfloat k2,
    jfloat chroma, jfloat vignette, jfloat renderScale, jboolean swapEyes,
    jint sbsMode, jfloat lensCx, jfloat lensCy) {
    if (!IsInitialized()) return;
    StereoProfile& p = GetHeadset().profile();
    p.eyeFovYDeg = fovYDeg;
    p.ipdMm = ipdMm;
    p.lensK1 = k1;
    p.lensK2 = k2;
    p.chromaticAberration = chroma;
    p.vignette = vignette;
    p.renderScale = renderScale;
    p.swapEyes = swapEyes == JNI_TRUE;
    p.sbsMode = static_cast<SbsMode>(sbsMode);
    p.lensCenterX = lensCx;
    p.lensCenterY = lensCy;
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetScreenSize(JNIEnv*, jobject,
                                                    jfloat wMm, jfloat hMm) {
    if (!IsInitialized()) return;
    GetHeadset().profile().screenPhysicalWidthMm = wMm;
    GetHeadset().profile().screenPhysicalHeightMm = hMm;
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeRecenter(JNIEnv*, jobject) {
    if (IsInitialized()) GetHeadset().recenter();
}

// ---------------------------------------------------------------------------
// tracking
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativePushImu(JNIEnv* env, jobject,
                                              jfloatArray gyro,
                                              jfloatArray accel,
                                              jfloatArray mag,
                                              jlong timestampNs) {
    if (!IsInitialized()) return;
    jfloat g[3], a[3];
    env->GetFloatArrayRegion(gyro, 0, 3, g);
    env->GetFloatArrayRegion(accel, 0, 3, a);
    Vec3 m;
    Vec3* mp = nullptr;
    if (mag) {
        jfloat mm[3];
        env->GetFloatArrayRegion(mag, 0, 3, mm);
        m = vec3(mm[0], mm[1], mm[2]);
        mp = &m;
    }
    GetTracking().pushImu(vec3(g[0], g[1], g[2]), vec3(a[0], a[1], a[2]), mp,
                          timestampNs);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetPoseFromExternal(
    JNIEnv*, jobject, jfloat px, jfloat py, jfloat pz, jfloat qx, jfloat qy,
    jfloat qz, jfloat qw, jlong ts, jfloat confidence) {
    if (!IsInitialized()) return;
    GetTracking().setPoseFromExternal(vec3(px, py, pz), quat(qx, qy, qz, qw),
                                      ts, confidence);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativePushCameraFrame(JNIEnv* env, jobject,
                                                      jbyteArray luma,
                                                      jint w, jint h,
                                                      jint stride,
                                                      jlong timestampNs) {
    if (!IsInitialized()) return;
    jbyte* elems = env->GetByteArrayElements(luma, nullptr);
    if (!elems) return;
    GetTracking().pushCameraFrame(
        reinterpret_cast<const uint8_t*>(elems), w, h, stride, timestampNs);
    env->ReleaseByteArrayElements(luma, elems, JNI_ABORT);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetTrackingBackend(JNIEnv*, jobject,
                                                         jint backend) {
    if (!IsInitialized()) return;
    GetTracking().setBackend(static_cast<TrackingBackend>(backend));
}

// config de filtros: [minCutoff, beta, dCutoff, useOneEuro, useKalman,
//                     useOrientationEkf, usePrediction, predictionTime,
//                     processNoise, measurementNoise]
JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetFilterConfig(JNIEnv* env, jobject,
                                                      jfloatArray cfg) {
    if (!IsInitialized()) return;
    jfloat c[10];
    env->GetFloatArrayRegion(cfg, 0, 10, c);
    PoseFilterConfig& p = GetTracking().filterConfig();
    p.oneEuro.minCutoffHz = c[0];
    p.oneEuro.beta = c[1];
    p.oneEuro.dCutoffHz = c[2];
    p.useOneEuro = c[3] > 0.5f;
    p.useKalmanPos = c[4] > 0.5f;
    p.useOrientationEkf = c[5] > 0.5f;
    p.usePrediction = c[6] > 0.5f;
    p.predictionTimeSec = c[7];
    p.kalmanPos.processNoiseAccel = c[8];
    p.kalmanPos.measurementNoise = c[9];
    GetTracking().applyFilterConfig();
}

JNIEXPORT jfloatArray JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeGetHeadPose(JNIEnv* env, jobject) {
    // [px,py,pz, qx,qy,qz,qw, velocity..., confidence, dofs]
    jfloatArray out = env->NewFloatArray(12);
    if (!IsInitialized() || !out) return out;
    const Pose& p = GetTracking().headPose();
    jfloat d[12] = {
        p.position.x, p.position.y, p.position.z,
        p.orientation.x, p.orientation.y, p.orientation.z, p.orientation.w,
        p.velocity.x, p.velocity.y, p.velocity.z,
        p.confidence, static_cast<jfloat>(p.dofs)
    };
    env->SetFloatArrayRegion(out, 0, 12, d);
    return out;
}

// ---------------------------------------------------------------------------
// mãos
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetHandLandmarks(
    JNIEnv* env, jobject, jboolean left, jfloatArray landmarks, jfloat confidence,
    jint gesture, jfloat pinchStrength, jlong timestampNs) {
    if (!IsInitialized()) return;
    Vec3 lm[kNumHandLandmarks];
    jfloat buf[kNumHandLandmarks * 3];
    env->GetFloatArrayRegion(landmarks, 0, kNumHandLandmarks * 3, buf);
    for (int i = 0; i < kNumHandLandmarks; ++i)
        lm[i] = vec3(buf[i * 3], buf[i * 3 + 1], buf[i * 3 + 2]);
    GetHands().setLandmarks(left == JNI_TRUE, lm, confidence, gesture,
                            pinchStrength, timestampNs);
}

// ---------------------------------------------------------------------------
// janelas espaciais
// ---------------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeCreateWindow(JNIEnv*, jobject, jint id,
                                                   jfloat px, jfloat py,
                                                   jfloat pz, jfloat qx,
                                                   jfloat qy, jfloat qz,
                                                   jfloat qw, jfloat widthM,
                                                   jfloat heightM,
                                                   jint content) {
    if (!IsInitialized()) return -1;
    return GetWindows().create(id, vec3(px, py, pz), quat(qx, qy, qz, qw),
                               widthM, heightM,
                               static_cast<WindowContent>(content));
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeUpdateWindow(JNIEnv* env, jobject,
                                                   jint windowId, jint field,
                                                   jfloatArray values) {
    if (!IsInitialized()) return;
    SpatialWindow* w = GetWindows().find(windowId);
    if (!w) return;
    jfloat v[8];
    jsize n = env->GetArrayLength(values);
    if (n > 8) n = 8;
    env->GetFloatArrayRegion(values, 0, n, v);
    switch (field) {
        case 0: // transform: px,py,pz,qx,qy,qz,qw,scale
            w->position = vec3(v[0], v[1], v[2]);
            w->orientation = quat(v[3], v[4], v[5], v[6]);
            w->scale = v[7];
            break;
        case 1: // size: w,h,alpha,curvature
            w->widthM = v[0];
            w->heightM = v[1];
            w->alpha = v[2];
            w->curvature = v[3];
            break;
        case 2: // flags: visible, focused, zOrder, textureId
            w->visible = v[0] > 0.5f;
            w->focused = v[1] > 0.5f;
            w->zOrder = static_cast<int>(v[2]);
            w->textureId = static_cast<GLuint>(static_cast<intptr_t>(v[3]));
            break;
        default: break;
    }
    (void)env;
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeRemoveWindow(JNIEnv*, jobject,
                                                   jint windowId) {
    if (IsInitialized()) GetWindows().remove(windowId);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeFocusWindow(JNIEnv*, jobject,
                                                  jint windowId) {
    if (IsInitialized()) GetWindows().focus(windowId);
}

// ---------------------------------------------------------------------------
// UI espacial
// ---------------------------------------------------------------------------
JNIEXPORT jint JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeAddUiPanel(JNIEnv* env, jobject,
                                                 jfloat px, jfloat py,
                                                 jfloat pz, jfloat qx,
                                                 jfloat qy, jfloat qz,
                                                 jfloat qw, jfloat widthM,
                                                 jfloat heightM,
                                                 jstring title,
                                                 jlong userData) {
    if (!IsInitialized()) return -1;
    UiPanel p;
    p.position = vec3(px, py, pz);
    p.orientation = quat(qx, qy, qz, qw);
    p.widthM = widthM;
    p.heightM = heightM;
    std::string t = toStr(env, title);
    if (!t.empty()) {
        p.hasTitle = true;
        std::strncpy(p.title, t.c_str(), sizeof(p.title) - 1);
    }
    p.userData = static_cast<uint64_t>(userData);
    return GetUiScene().addPanel(p);
}

JNIEXPORT jint JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeAddUiControl(JNIEnv* env, jobject,
                                                   jint panelId, jint kind,
                                                   jfloat x, jfloat y,
                                                   jfloat w, jfloat h,
                                                   jstring label, jfloat value,
                                                   jint iconTexture) {
    if (!IsInitialized()) return -1;
    UiControl c;
    c.kind = static_cast<UiControlKind>(kind);
    c.x = x; c.y = y; c.w = w; c.h = h;
    std::string l = toStr(env, label);
    std::strncpy(c.label, l.c_str(), sizeof(c.label) - 1);
    c.value = value;
    c.iconTexture = static_cast<GLuint>(iconTexture);
    return GetUiScene().addControl(panelId, c);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetUiControlValue(JNIEnv*, jobject,
                                                        jint panelId,
                                                        jint controlId,
                                                        jfloat value) {
    if (IsInitialized()) GetUiScene().setControlValue(panelId, controlId, value);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetUiControlLabel(JNIEnv* env, jobject,
                                                        jint panelId,
                                                        jint controlId,
                                                        jstring label) {
    if (IsInitialized())
        GetUiScene().setControlLabel(panelId, controlId, toStr(env, label).c_str());
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetUiControlVisible(JNIEnv*, jobject,
                                                          jint panelId,
                                                          jint controlId,
                                                          jboolean visible) {
    if (IsInitialized())
        GetUiScene().setControlVisible(panelId, controlId, visible == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetUiPanelTransform(JNIEnv*, jobject,
                                                          jint panelId,
                                                          jfloat px, jfloat py,
                                                          jfloat pz, jfloat qx,
                                                          jfloat qy, jfloat qz,
                                                          jfloat qw) {
    if (IsInitialized())
        GetUiScene().setPanelTransform(panelId, vec3(px, py, pz),
                                       quat(qx, qy, qz, qw));
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetUiPanelVisible(JNIEnv*, jobject,
                                                        jint panelId,
                                                        jboolean visible) {
    if (IsInitialized())
        GetUiScene().setPanelVisible(panelId, visible == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeRemoveUiPanel(JNIEnv*, jobject,
                                                    jint panelId) {
    if (IsInitialized()) GetUiScene().removePanel(panelId);
}

// ---------------------------------------------------------------------------
// atlas de fonte: Kotlin gera o bitmap (Paint), faz upload (GLES30) e passa
// o texId + métricas. Layout de metrics:
//   [count, baseHeightPx, (code,x,y,w,h,xoff,yoff,advance) * count]
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetFontAtlas(JNIEnv* env, jobject,
                                                   jint texId, jint atlasW,
                                                   jint atlasH,
                                                   jfloatArray metrics) {
    if (!IsInitialized()) return;
    jsize n = env->GetArrayLength(metrics);
    if (n < 2) return;
    std::vector<jfloat> m(n);
    env->GetFloatArrayRegion(metrics, 0, n, m.data());
    int count = static_cast<int>(m[0]);
    float baseHeight = m[1];
    std::vector<GlyphInfo> glyphs;
    glyphs.reserve(static_cast<std::size_t>(count));
    const int stride = 8;
    for (int i = 0; i < count && (2 + (i + 1) * stride) <= n; ++i) {
        GlyphInfo g;
        g.code = static_cast<uint16_t>(m[2 + i * stride + 0]);
        g.x = m[2 + i * stride + 1];
        g.y = m[2 + i * stride + 2];
        g.w = m[2 + i * stride + 3];
        g.h = m[2 + i * stride + 4];
        g.xoff = m[2 + i * stride + 5];
        g.yoff = m[2 + i * stride + 6];
        g.advance = m[2 + i * stride + 7];
        glyphs.push_back(g);
    }
    SetFontAtlas(static_cast<GLuint>(texId), atlasW, atlasH, baseHeight, glyphs);
}

// ---------------------------------------------------------------------------
// GLB / modelos
// ---------------------------------------------------------------------------
JNIEXPORT jlong JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeLoadGlb(JNIEnv* env, jobject,
                                              jbyteArray data, jlong size) {
    if (!IsInitialized()) return 0;
    jbyte* elems = env->GetByteArrayElements(data, nullptr);
    if (!elems) return 0;
    std::string err;
    auto model = LoadGLB(reinterpret_cast<const uint8_t*>(elems),
                         static_cast<std::size_t>(size), err);
    env->ReleaseByteArrayElements(data, elems, JNI_ABORT);
    if (!model) {
        BMR_LOGE("loadGlb falhou: %s", err.c_str());
        return 0;
    }
    model->userId = reinterpret_cast<uint64_t>(model.get()); // handle
    // registra na cena invisível (a SDK Lua/C++ posiciona depois)
    GetScene().addModel(model, Vec3{0, 1.2f, -2.5f});
    auto* obj = GetScene().find(GetScene().objects().empty() ? 0 :
                                GetScene().objects().back().id);
    if (obj) obj->visible = false; // escondido até a API posicionar
    return reinterpret_cast<jlong>(model.get());
}

JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetModelTransform(JNIEnv*, jobject,
                                                        jlong modelHandle,
                                                        jfloat px, jfloat py,
                                                        jfloat pz, jfloat qx,
                                                        jfloat qy, jfloat qz,
                                                        jfloat qw, jfloat sx,
                                                        jfloat sy, jfloat sz,
                                                        jboolean visible) {
    if (!IsInitialized()) return JNI_FALSE;
    for (auto& o : GetScene().objects()) {
        if (o.model && reinterpret_cast<jlong>(o.model.get()) == modelHandle) {
            o.position = vec3(px, py, pz);
            o.orientation = quat(qx, qy, qz, qw);
            o.scale = vec3(sx, sy, sz);
            o.visible = visible == JNI_TRUE;
            return JNI_TRUE;
        }
    }
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetModelAnimation(JNIEnv*, jobject,
                                                        jlong modelHandle,
                                                        jint animIndex,
                                                        jboolean loop) {
    if (!IsInitialized()) return JNI_FALSE;
    for (auto& o : GetScene().objects()) {
        if (o.model && reinterpret_cast<jlong>(o.model.get()) == modelHandle) {
            o.animIndex = animIndex;
            o.animLoop = loop == JNI_TRUE;
            return JNI_TRUE;
        }
    }
    return JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeGetModelAnimationCount(JNIEnv*, jobject,
                                                             jlong modelHandle) {
    if (!IsInitialized()) return 0;
    for (auto& o : GetScene().objects())
        if (o.model && reinterpret_cast<jlong>(o.model.get()) == modelHandle)
            return o.model->animationCount();
    return 0;
}

JNIEXPORT jstring JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeGetModelAnimationName(JNIEnv* env, jobject,
                                                            jlong modelHandle,
                                                            jint index) {
    if (!IsInitialized()) return env->NewStringUTF("");
    for (auto& o : GetScene().objects())
        if (o.model && reinterpret_cast<jlong>(o.model.get()) == modelHandle)
            return env->NewStringUTF(o.model->animationName(index).c_str());
    return env->NewStringUTF("");
}

// ---------------------------------------------------------------------------
// input / ponteiros
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativePushPointer(JNIEnv*, jobject, jint source,
                                                  jfloat ox, jfloat oy,
                                                  jfloat oz, jfloat dx,
                                                  jfloat dy, jfloat dz,
                                                  jint pointerEvent) {
    if (!IsInitialized()) return;
    PushPointer(static_cast<PointerSource>(source), vec3(ox, oy, oz),
                vec3(dx, dy, dz), pointerEvent);
}

JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetActiveRay(JNIEnv*, jobject,
                                                   jboolean visible, jfloat ox,
                                                   jfloat oy, jfloat oz,
                                                   jfloat dx, jfloat dy,
                                                   jfloat dz, jfloat r,
                                                   jfloat g, jfloat b) {
    if (!IsInitialized()) return;
    SetActiveRay(visible == JNI_TRUE, vec3(ox, oy, oz), vec3(dx, dy, dz),
                 vec3(r, g, b));
}

// ---------------------------------------------------------------------------
// spawn (SDK Lua/C++)
// ---------------------------------------------------------------------------
JNIEXPORT jlong JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSpawnSphere(JNIEnv*, jobject, jfloat x,
                                                  jfloat y, jfloat z,
                                                  jfloat radius) {
    if (!IsInitialized()) return 0;
    return GetScene().spawnSphere(vec3(x, y, z), radius);
}

JNIEXPORT jlong JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSpawnCube(JNIEnv*, jobject, jfloat x,
                                                jfloat y, jfloat z,
                                                jfloat halfExtent) {
    if (!IsInitialized()) return 0;
    return GetScene().spawnCube(vec3(x, y, z), halfExtent);
}

JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetObjectState(JNIEnv* env, jobject,
                                                     jlong id, jint field,
                                                     jfloatArray values) {
    if (!IsInitialized()) return JNI_FALSE;
    SpawnedObject* o = GetScene().find(static_cast<uint64_t>(id));
    if (!o) return JNI_FALSE;
    jsize n = env->GetArrayLength(values);
    jfloat v[10];
    if (n > 10) n = 10;
    env->GetFloatArrayRegion(values, 0, n, v);
    switch (field) {
        case 0: // transform pos/quat/scale
            o->position = vec3(v[0], v[1], v[2]);
            o->orientation = quat(v[3], v[4], v[5], v[6]);
            o->scale = vec3(v[7], v[8], v[9]);
            break;
        case 1: // física: velocity, gravity, radius
            o->velocity = vec3(v[0], v[1], v[2]);
            o->gravity = v[3] > 0.5f;
            o->radius = v[4];
            break;
        case 2: // cor
            o->color[0] = v[0]; o->color[1] = v[1];
            o->color[2] = v[2]; o->color[3] = v[3];
            break;
        default: return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeRemoveObject(JNIEnv*, jobject, jlong id) {
    if (!IsInitialized()) return JNI_FALSE;
    return GetScene().removeObject(static_cast<uint64_t>(id));
}

// ---------------------------------------------------------------------------
// performance / térmico
// ---------------------------------------------------------------------------
JNIEXPORT void JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeSetThermalBudget(JNIEnv*, jobject,
                                                       jfloat budget01) {
    SetThermalBudget(budget01);
}

JNIEXPORT jfloatArray JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeGetStats(JNIEnv* env, jobject) {
    jfloatArray out = env->NewFloatArray(7);
    if (!out) return nullptr;
    if (IsInitialized()) {
        float s[7];
        GetStats(s);
        env->SetFloatArrayRegion(out, 0, 7, s);
    }
    return out;
}

// ---------------------------------------------------------------------------
// frame
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativeRenderFrame(JNIEnv*, jobject,
                                                  jfloat dtSec, jlong nowNs) {
    return RenderFrame(dtSec, nowNs) ? JNI_TRUE : JNI_FALSE;
}

// ---------------------------------------------------------------------------
// eventos (drain)
// ---------------------------------------------------------------------------
JNIEXPORT jboolean JNICALL
Java_com_brazilmr_sdk_NativeSdk_nativePollEvent(JNIEnv* env, jobject,
                                                jintArray outEvent) {
    if (!IsInitialized()) return JNI_FALSE;
    VrEvent ev;
    if (!PollEvent(ev)) return JNI_FALSE;
    jint data[6] = {
        static_cast<jint>(ev.type), ev.a, ev.b,
        static_cast<jint>(ev.x * 1000.0f),
        static_cast<jint>(ev.y * 1000.0f),
        static_cast<jint>(ev.z * 1000.0f)
    };
    env->SetIntArrayRegion(outEvent, 0, 6, data);
    return JNI_TRUE;
}

} // extern "C"
