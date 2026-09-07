// BrazilMR — UI espacial nativa.
// Painéis flutuantes com controles (botões, sliders, labels, toggles, ícones).
// Hit-testing por raycast no próprio runtime → eventos via JNI para Kotlin/Lua.
// Nada aqui é "tela 2D": tudo vive no espaço 3D com profundidade e foco.
#pragma once

#include "../math/MathTypes.hpp"
#include "../gl/GlHeaders.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

namespace brazilmr {

enum class UiControlKind : int {
    BUTTON = 0,
    SLIDER = 1,
    LABEL  = 2,
    TOGGLE = 3,
    ICON   = 4,   // textura (ícone de app)
};

struct UiControl {
    int id = 0;
    UiControlKind kind = UiControlKind::BUTTON;
    float x = 0, y = 0, w = 0.2f, h = 0.06f;  // metros, origem topo-esq do painel
    char label[96] = {0};
    float value = 0.0f;       // slider/toggle
    bool hovered = false;
    bool pressed = false;
    GLuint iconTexture = 0;   // ICON
    float iconUv[4] = {0, 0, 1, 1};
    bool visible = true;
};

struct UiPanel {
    int id = 0;
    Vec3 position{0, 1.4f, -1.6f};
    Quat orientation = Quat::identity();
    float widthM = 0.8f;
    float heightM = 0.5f;
    bool visible = true;
    bool focusable = true;
    bool hasTitle = false;
    char title[96] = {0};
    uint64_t userData = 0;    // handle Lua/C++ associado
    std::vector<UiControl> controls;
};

// Eventos UI → camada superior (JNI → Kotlin → Lua/handlers)
enum UiEventType : int {
    UI_EVENT_NONE = 0,
    UI_EVENT_BUTTON = 1,
    UI_EVENT_SLIDER = 2,
    UI_EVENT_TOGGLE = 3,
    UI_EVENT_HOVER = 4,
    UI_EVENT_PANEL_FOCUS = 5,
};

struct UiEvent {
    UiEventType type = UI_EVENT_NONE;
    int panelId = 0;
    int controlId = 0;
    float value = 0.0f;
};

enum class UiPointerEvent : int {
    HOVER = 0,
    PRESS = 1,
    RELEASE = 2,
    MOVE = 3,
    CANCEL = 4,
};

struct UiHit {
    bool hit = false;
    int panelId = -1;
    int controlId = -1;
    float u = 0.0f, v = 0.0f;   // dentro do painel
    float distance = 0.0f;
    Vec3 point{0, 0, 0};
};

class UiScene {
public:
    int addPanel(const UiPanel& panel);
    bool removePanel(int panelId);
    void clear();

    int addControl(int panelId, const UiControl& control);
    bool setControlValue(int panelId, int controlId, float value);
    bool setControlLabel(int panelId, int controlId, const char* label);
    bool setControlVisible(int panelId, int controlId, bool visible);
    bool setPanelTransform(int panelId, const Vec3& pos, const Quat& rot);
    bool setPanelVisible(int panelId, bool visible);
    UiPanel* panel(int panelId);

    std::vector<UiPanel>& panels() { return panels_; }
    const std::vector<UiPanel>& panels() const { return panels_; }

    // Raycast contra os painéis visíveis.
    UiHit hitTest(const Vec3& origin, const Vec3& dir) const;

    // Dispatch de evento de ponteiro (gaze/mão/controller).
    // Gera eventos de UI (clique, slider drag…) em outEvents.
    void dispatchPointer(const Vec3& origin, const Vec3& dir,
                         UiPointerEvent ev, std::vector<UiEvent>& outEvents);

    // Painel com hover ativo (para feedback visual).
    int hoveredPanel() const { return hoveredPanel_; }
    int hoveredControl() const { return hoveredControl_; }

private:
    int nextPanelId_ = 1;
    int nextControlId_ = 1;
    std::vector<UiPanel> panels_;
    int hoveredPanel_ = -1;
    int hoveredControl_ = -1;
    int pressedControl_ = -1;
    int pressedPanel_ = -1;
    float sliderGrabOffset_ = 0.0f;
};

// ---------------------------------------------------------------------------
// Atlas de texto (gerado em Kotlin via android.graphics.Paint, upload nativo)
// ---------------------------------------------------------------------------
struct GlyphInfo {
    uint16_t code = 0;
    float x = 0, y = 0, w = 0, h = 0;   // no atlas (px)
    float xoff = 0, yoff = 0;           // offsets de desenho (px @ altura base)
    float advance = 0;                  // avanço horizontal (px @ altura base)
};

class TextAtlas {
public:
    void set(GLuint texId, int atlasW, int atlasH, float baseHeightPx,
             const std::vector<GlyphInfo>& glyphs);

    float measureText(const char* utf8, float heightM) const;
    // Adiciona quads (pos2+uv2+rgba4 por vértice) ao buffer. Retorna avanço.
    float buildQuads(const char* utf8, float xMeters, float yMeters,
                     float heightMeters, const float rgba[4],
                     std::vector<float>& outQuads) const;

    bool valid() const { return texture_ != 0; }
    GLuint texture() const { return texture_; }
    float baseHeight() const { return baseHeightPx_; }

private:
    const GlyphInfo* findGlyph(uint32_t code) const;
    uint32_t nextUtf8(const char* s, int& byteIdx) const;

    GLuint texture_ = 0;
    int atlasW_ = 0, atlasH_ = 0;
    float baseHeightPx_ = 64.0f;
    std::vector<GlyphInfo> glyphs_;
};

} // namespace brazilmr
