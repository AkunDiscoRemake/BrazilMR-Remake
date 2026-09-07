// BrazilMR — janelas flutuantes 3D.
// Cada janela vive no espaço VR: posição, rotação, escala, profundidade,
// foco e z-order. O raycast (mão/controller/gaze) resolve o pixel exato
// da superfície sob o cursor → eventos de input espacial.
#pragma once

#include "../math/MathTypes.hpp"
#include "../gl/GlHeaders.hpp"
#include <cstdint>
#include <vector>

namespace brazilmr {

enum class WindowContent : int {
    PLACEHOLDER = 0,   // sem conteúdo ainda
    APP_SURFACE = 1,   // app Android (MediaProjection / bridge)
    BROWSER     = 2,   // WebView do navegador VR
    EMBEDDED    = 3,   // View embutida (terminal, teclado)
    LUA_UI      = 4,   // painel criado pela SDK Lua
};

struct SpatialWindow {
    int id = 0;
    Vec3 position{0, 0, -2};
    Quat orientation = Quat::identity();
    float scale = 1.0f;
    float widthM = 1.1f;      // largura física em metros
    float heightM = 0.6875f;  // altura física (16:10 default)
    float curvature = 0.12f;  // curvatura cilíndrica (0 = plana)
    bool  focused = false;
    bool  visible = true;
    int   zOrder = 0;
    WindowContent content = WindowContent::PLACEHOLDER;
    GLuint textureId = 0;     // external OES (SurfaceTexture) — 0 = placeholder
    float contentAspect = 1.6f;
    float alpha = 1.0f;
    bool  followHead = false; // dock fixo ao olhar (menus)
    float birthTime = 0.0f;   // animação de abertura
};

// Resultado de um raycast contra janelas.
struct WindowHit {
    bool hit = false;
    int windowId = -1;
    float distance = 0.0f;
    float u = 0.0f, v = 0.0f;  // 0..1 dentro da janela (v=0 topo)
    Vec3 point{0, 0, 0};
};

// Interseção raio × janela (quase plana; a curvatura é desconsiderada no
// hit-test — erro < 2% da área, imperceptível para apontar).
inline WindowHit raycastWindow(const Vec3& origin, const Vec3& dir,
                               const SpatialWindow& w) {
    WindowHit out;
    if (!w.visible) return out;
    // base ortonormal da janela
    Vec3 normal = w.orientation.rotate(Vec3{0, 0, 1});
    Vec3 right  = w.orientation.rotate(Vec3{1, 0, 0});
    Vec3 up     = w.orientation.rotate(Vec3{0, 1, 0});
    float t = rayPlane(origin, dir, w.position, normal);
    if (t < 0.0f) return out;
    Vec3 p = origin + dir * t;
    Vec3 local = p - w.position;
    float lx = local.dot(right) / (w.widthM * 0.5f * w.scale);
    float ly = local.dot(up) / (w.heightM * 0.5f * w.scale);
    if (std::fabs(lx) > 1.0f || std::fabs(ly) > 1.0f) return out;
    out.hit = true;
    out.windowId = w.id;
    out.distance = t;
    out.u = (lx + 1.0f) * 0.5f;
    out.v = (1.0f - ly) * 0.5f; // v=0 topo
    out.point = p;
    return out;
}

// Raycast contra todas as janelas ordenadas por z (a mais próxima ganha).
inline WindowHit raycastWindows(const Vec3& origin, const Vec3& dir,
                                const std::vector<SpatialWindow>& windows) {
    WindowHit best;
    for (const auto& w : windows) {
        WindowHit h = raycastWindow(origin, dir, w);
        if (h.hit && (!best.hit || h.distance < best.distance)) best = h;
    }
    return best;
}

// Layout em arco confortável (dock da home).
void layoutArc(std::vector<SpatialWindow>& windows, float radiusM,
               float centerAngleDeg, float spreadDeg);

} // namespace brazilmr
