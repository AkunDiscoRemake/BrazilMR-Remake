# BrazilMR — Plataforma VR nativa para VR Box / Cardboard

Plataforma Android de realidade virtual **100% espacial**, desenhada
exclusivamente para headsets **VR Box / Google Cardboard**. Não é um
launcher 2D, não é "Android com duas telas": o aparelho entra direto no
modo estéreo SBS e toda a experiência — sistema, apps, navegador,
teclado — acontece em janelas flutuantes 3D com profundidade.

> **Regra de ouro do projeto**: na dúvida entre "interface 2D do Android
> que funciona em VR" e "experiência VR espacial nativa para VR Box" —
> **sempre a opção B**.

---

## Visão geral

| Camada | O quê | Onde |
|---|---|---|
| **Motor nativo C++17** | Renderer estéreo, distorção de lente, fusão IMU, SLAM CV, filtros, GLB, física, UI espacial, compositor | `app/src/main/cpp/brazilmr/` |
| **Ponte JNI** | 44 funções `native*` (superfície completa em `JniBridge.cpp`) | `app/src/main/cpp/brazilmr_jni/` |
| **Plataforma Kotlin** | Activity única, sensores, Camera2, MediaPipe, MediaProjection, LuaJ, térmico | `app/src/main/java/com/brazilmr/` |
| **SDKs** | C++ (`VR::Initialize()`...) e Lua (`vr.createWindow`...) | `cpp/brazilmr/sdk/`, `java/.../sdk/lua/` |

### Pipeline central (tudo nativo, 60 Hz)

```
HEADSET → SENSORES → FUSÃO IMU (One Euro→Kalman/EKF→predição)
        → CÂMERA (Camera2 YUV) → KLT piramidal → VO/RANSAC → 6DoF CV
        → POSE (late-latch anti-latência)
        → RENDER ESTÉREO (sky→modelos→janelas→UI→mãos→raio)
        → LENTE (barrel k1/k2 + cromática + vignette) → COMPOSITOR SBS
```

- **3DoF sempre** (giro+accel+mag, fusão complementar em quatérnions com
  correções no frame correto: corpo vs. mundo).
- **6DoF quando possível**: ARCore (primário) → odometria visual própria
  (FAST-9 + bucketing por célula + KLT piramidal + essencial 8-pt +
  RANSAC + cheirality) → 3DoF IMU. Nunca cai para 2D.
- **Mãos quando possível**: MediaPipe HandLandmarker (LIVE_STREAM,
  21 landmarks, gestos PINCH/GRAB/FIST/POINT/OPEN_PALM/SWIPE derivados
  dos landmarks) → controller genérico → ponteiro de cabeça (0DoF).
- **Térmico**: escada nativa res→mãos→SLAM→escala→FPS→efeitos guiada
  pelo orçamento 0..1 que o Kotlin calcula de temperatura/FPS/throttle.

### Janelas espaciais e apps Android

```
APP → BRIDGE (VrProjectionService/MediaProjection ou ADB opcional)
    → CAPTURA (VirtualDisplay→ImageReader RGBA)
    → JANELA ESPACIAL (CONTENT_APP_SURFACE, z-order/foco/profundidade)
    → INPUT (raycast→tap via AccessibilityService.dispatchGesture,
             drag→swipe, pinch→tap, controller→toque virtual)
    → ESTÉREO
```

O usuário lança apps pela **biblioteca espacial** (ícones 3D), e os usa
sem nunca sair do VR.

---

## Estrutura

```
app/src/main/
├── cpp/
│   ├── brazilmr/
│   │   ├── core/        Log, Clock, Pool
│   │   ├── math/        Vec3/Quat/Mat4, Euler YXZ
│   │   ├── filters/     One Euro, Kalman/EKF, pipeline RAW→…→predição
│   │   ├── tracking/    IMU, fluxo óptico (FAST/KLT), VO, tipos
│   │   ├── gl/          headers GLES3 + utilitários
│   │   ├── renderer/    StereoConfig, DistortionMesh, Camera, Sky,
│   │   │                Model/Window/Hand/Ui/Stereo renderers, Compositor
│   │   ├── windows/     SpatialWindow (pos/rot/escala/z/foco/curvatura)
│   │   ├── hands/       HandTypes (21 landmarks, gestos)
│   │   ├── ui/          UiScene (painéis, botões, sliders, hit-test)
│   │   ├── glb/         JSON, GLB parser, runtime glTF (anims/skins/PBR)
│   │   └── sdk/         VR.hpp — facade C++ da plataforma
│   ├── brazilmr_jni/    JniBridge.cpp (superfície JNI única)
│   ├── CMakeLists.txt   libbrazilmr.so
│   └── host_tests/      suite de testes para host (sem Android)
├── java/com/brazilmr/
│   ├── platform/        VrActivity, VrOs, FontAtlas, HeadsetProfiles
│   ├── tracking/        ImuTracker (remap device→head)
│   ├── camera/          Camera2Engine (YUV, presets, buffer reuse)
│   ├── hands/           HandTracking (MediaPipe + gestos + projeção)
│   ├── input/           InputRouter (gaze/controller/teclado físico)
│   ├── browser/         VrBrowser (WebView→textura), SpatialKeyboard
│   ├── apps/            AppLibrary, AppBridge
│   │   └── bridge/      VrProjectionService, VrAccessibilityService
│   ├── performance/     ThermalManager
│   ├── ui/              CalibrationWizard (8 passos, dentro do VR)
│   └── sdk/             NativeSdk (JNI 1:1), lua/LuaSdk, VrPrefs
└── assets/
    ├── models/brazilmr_logo.glb   (gerado por tools/gen_test_glb.py)
    ├── device_profiles.json       (perfis VR Box/Cardboard/BoboVR…)
    └── lua/                       (exemplos da SDK Lua)
```

---

## Build

Requisitos: **JDK 17, Android SDK 34, NDK 26.1.10909125, CMake 3.22.1**.

```bash
# 1) modelo de hand tracking (opcional — sem ele usa ponteiro de cabeça)
tools/fetch_models.sh

# 2) APK
./gradlew :app:assembleDebug

# 3) instalar e colocar o celular no VR Box
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- AGP 8.5.2 / Kotlin 1.9.24 / minSdk 26 / target 34 / `c++_shared`
- Flags nativos: `-std=c++17 -O3 -fno-exceptions -fvisibility=hidden`
- ABIs: `arm64-v8a`, `armeabi-v7a`
- **ARCore é opcional** (meta-data `optional`): ausente → CV próprio.
- **LuaJ 3.0.1** é a engine padrão; **LuaJIT** nativo é opcional
  (`-DBRAZILMR_BUILD_LUAJIT=ON` + libluajit pré-compilada).

### Permissões (todas pedidas no pré-voo escuro, 1ª execução)

CÂMERA (tracking/mãos) · MediaProjection (apps em janelas) ·
AccessibilityService (injeção de toque nos apps, habilitado nas
configurações do sistema pelo wizard) — depois disso, tudo acontece no VR.

---

## Testes (host — sem Android/emulador)

O motor nativo inteiro compila e roda **no host** (Linux/macOS com g++):

```bash
cd app/src/main/cpp/host_tests
make run     # compila o motor completo + stubs GL/JNI e executa
make syntax  # checagem de sintaxe por arquivo
make clean
```

**101 verificações, 0 falhas** — matemática, One Euro, Kalman/EKF,
pipeline de pose, fusão IMU (frame corpo/mundo), KLT piramidal (bugs
clássicos de referência/propagação testados), odometria visual
(essencial+RANSAC+cheirality), malha de distorção (convenção Cardboard),
JSON, GLB/glTF (malhas, materiais, animações slerp), UI espacial
(hit-test, foco, sliders, eventos).

Gerador do modelo de teste:

```bash
python3 tools/gen_test_glb.py   # valida GLB 2.0 e regenera os .glb
```

---

## SDK

### C++ (link com `libbrazilmr.so`)

```cpp
#include "brazilmr/sdk/VR.hpp"
using namespace brazilmr;
VR::Initialize({.screenW = 1920, .screenH = 1080});
VR::GetHeadset().profile().ipdMm = 64.0f;
auto model = VR::LoadGlb(data, size);       // handle
VR::SetModelTransform(model, pos, quat, scale, true);
VR::PushPointer(PointerSource::GAZE, origin, dir, 1 /*press*/);
VR::RenderFrame(dt, nowNs);                 // frame estéreo completo
```

### Lua (executa dentro do VR — ver `assets/lua/`)

```lua
local m = vr.loadModel("models/brazilmr_logo.glb")
vr.setModelTransform(m, 0, 0.5, -1.9, 15, 0.4, true)
local id = vr.spawnSphere(0, 1.5, -2, 0.08)
vr.setColor(id, 0, 0.9, 0.63, 1)
vr.onGesture(function(hand, g, s)
  if g == "PINCH" then vr.log("pinça!") end
end)
```

API: `createWindow/removeWindow/setWindowTransform/loadModel/
setModelTransform/setModelAnimation/spawnSphere/spawnCube/setColor/
removeObject/getHeadPose/getHand/raycast/onGesture/onEvent/
setThermalBudget/recenter/log`.

### JNI (Kotlin ↔ nativo)

`com.brazilmr.sdk.NativeSdk` declara as 44 `external fun` **1:1** com
`JniBridge.cpp` (fonte da verdade). Utilitários tipados: `HeadPose`,
`setFilterConfig(FILTER_BALANCED|STABLE|RESPONSIVE)`, `uploadBitmap`,
wrappers de janela/UI/objetos.

---

## Calibração (wizard 8 passos, 100% em VR)

Posicionamento → lentes (grid SBS_CALIBRATION) → IPD (slider,
52–78 mm) → horizonte (recenter) → tracking → mãos → controle →
entra no VR (persiste em `SharedPreferences`).

## Atalhos universais no headset

- **Toque na tela** = clique do ponteiro ativo (gaze/mão/controller)
- **Toque longo / Volume** = recentrar vista
- **Palma aberta** (com mãos) = alterna menu principal
- **Gamepad A** (Joy-Con etc.) = clique · stick = navegação
