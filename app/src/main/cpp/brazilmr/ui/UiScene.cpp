#include "UiScene.hpp"

namespace brazilmr {

// ---------------------------------------------------------------------------
// UiScene
// ---------------------------------------------------------------------------
int UiScene::addPanel(const UiPanel& panel) {
    UiPanel p = panel;
    p.id = nextPanelId_++;
    panels_.push_back(p);
    return p.id;
}

bool UiScene::removePanel(int panelId) {
    for (auto it = panels_.begin(); it != panels_.end(); ++it) {
        if (it->id == panelId) {
            panels_.erase(it);
            if (hoveredPanel_ == panelId) { hoveredPanel_ = -1; hoveredControl_ = -1; }
            return true;
        }
    }
    return false;
}

void UiScene::clear() {
    panels_.clear();
    hoveredPanel_ = hoveredControl_ = pressedPanel_ = pressedControl_ = -1;
}

int UiScene::addControl(int panelId, const UiControl& control) {
    for (auto& p : panels_) {
        if (p.id != panelId) continue;
        UiControl c = control;
        c.id = nextControlId_++;
        p.controls.push_back(c);
        return c.id;
    }
    return -1;
}

bool UiScene::setControlValue(int panelId, int controlId, float value) {
    for (auto& p : panels_) {
        if (p.id != panelId) continue;
        for (auto& c : p.controls)
            if (c.id == controlId) { c.value = value; return true; }
    }
    return false;
}

bool UiScene::setControlLabel(int panelId, int controlId, const char* label) {
    for (auto& p : panels_) {
        if (p.id != panelId) continue;
        for (auto& c : p.controls) {
            if (c.id == controlId) {
                std::strncpy(c.label, label, sizeof(c.label) - 1);
                c.label[sizeof(c.label) - 1] = '\0';
                return true;
            }
        }
    }
    return false;
}

bool UiScene::setControlVisible(int panelId, int controlId, bool visible) {
    for (auto& p : panels_) {
        if (p.id != panelId) continue;
        for (auto& c : p.controls)
            if (c.id == controlId) { c.visible = visible; return true; }
    }
    return false;
}

bool UiScene::setPanelTransform(int panelId, const Vec3& pos, const Quat& rot) {
    for (auto& p : panels_) {
        if (p.id == panelId) { p.position = pos; p.orientation = rot; return true; }
    }
    return false;
}

bool UiScene::setPanelVisible(int panelId, bool visible) {
    for (auto& p : panels_) {
        if (p.id == panelId) { p.visible = visible; return true; }
    }
    return false;
}

UiPanel* UiScene::panel(int panelId) {
    for (auto& p : panels_)
        if (p.id == panelId) return &p;
    return nullptr;
}

UiHit UiScene::hitTest(const Vec3& origin, const Vec3& dir) const {
    UiHit best;
    for (const auto& p : panels_) {
        if (!p.visible) continue;
        Vec3 normal = p.orientation.rotate(Vec3{0, 0, 1});
        float t = rayPlane(origin, dir, p.position, normal);
        if (t < 0.0f) continue;
        Vec3 point = origin + dir * t;
        Vec3 local = point - p.position;
        float lu = local.dot(p.orientation.rotate(Vec3{1, 0, 0})) / p.widthM;
        float lv = local.dot(p.orientation.rotate(Vec3{0, 1, 0})) / p.heightM;
        float u = lu + 0.5f;
        float v = 0.5f - lv;
        if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) continue;
        if (!best.hit || t < best.distance) {
            best.hit = true;
            best.panelId = p.id;
            best.controlId = -1;
            best.u = u;
            best.v = v;
            best.distance = t;
            best.point = point;
        }
    }
    if (best.hit) {
        // refina para controle
        const UiPanel* p = nullptr;
        for (auto& pp : panels_) if (pp.id == best.panelId) p = &pp;
        if (p) {
            float lx = best.u * p->widthM;
            float ly = best.v * p->heightM;
            for (auto& c : p->controls) {
                if (!c.visible) continue;
                if (lx >= c.x && lx <= c.x + c.w && ly >= c.y && ly <= c.y + c.h) {
                    best.controlId = c.id;
                    break;
                }
            }
        }
    }
    return best;
}

void UiScene::dispatchPointer(const Vec3& origin, const Vec3& dir,
                              UiPointerEvent ev, std::vector<UiEvent>& outEvents) {
    UiHit hit = hitTest(origin, dir);

    // hover change
    int newPanel = hit.hit ? hit.panelId : -1;
    int newControl = hit.hit ? hit.controlId : -1;
    if (newPanel != hoveredPanel_ || newControl != hoveredControl_) {
        if (newControl >= 0 && ev == UiPointerEvent::HOVER) {
            UiEvent e;
            e.type = UI_EVENT_HOVER;
            e.panelId = newPanel;
            e.controlId = newControl;
            outEvents.push_back(e);
        }
        hoveredPanel_ = newPanel;
        hoveredControl_ = newControl;
    }
    // atualiza flags de hover nos controles
    for (auto& p : panels_) {
        for (auto& c : p.controls) c.hovered = (p.id == hoveredPanel_ && c.id == hoveredControl_);
    }

    switch (ev) {
        case UiPointerEvent::PRESS: {
            if (!hit.hit) { pressedControl_ = -1; pressedPanel_ = -1; break; }
            pressedControl_ = hit.controlId;
            pressedPanel_ = hit.panelId;
            if (hit.controlId < 0) {
                UiEvent e;
                e.type = UI_EVENT_PANEL_FOCUS;
                e.panelId = hit.panelId;
                outEvents.push_back(e);
                break;
            }
            for (auto& p : panels_) {
                if (p.id != hit.panelId) continue;
                for (auto& c : p.controls) {
                    if (c.id != hit.controlId) continue;
                    c.pressed = true;
                    if (c.kind == UiControlKind::SLIDER) {
                        // pega o knob pela posição do toque
                        float knobX = c.x + c.w * c.value;
                        sliderGrabOffset_ = (hit.u * p.widthM) - knobX;
                    }
                }
            }
            break;
        }
        case UiPointerEvent::MOVE:
        case UiPointerEvent::HOVER: {
            if (pressedControl_ >= 0 && pressedPanel_ >= 0) {
                for (auto& p : panels_) {
                    if (p.id != pressedPanel_) continue;
                    for (auto& c : p.controls) {
                        if (c.id != pressedControl_) continue;
                        if (c.kind == UiControlKind::SLIDER && hit.hit && hit.panelId == pressedPanel_) {
                            float x = hit.u * p.widthM - sliderGrabOffset_;
                            float v = clampf((x - c.x) / c.w, 0.0f, 1.0f);
                            c.value = v;
                            UiEvent e;
                            e.type = UI_EVENT_SLIDER;
                            e.panelId = p.id;
                            e.controlId = c.id;
                            e.value = v;
                            outEvents.push_back(e);
                        }
                    }
                }
            }
            break;
        }
        case UiPointerEvent::RELEASE: {
            if (pressedControl_ >= 0) {
                for (auto& p : panels_) {
                    for (auto& c : p.controls) {
                        if (c.id != pressedControl_) continue;
                        c.pressed = false;
                        // só dispara se soltou sobre o mesmo controle
                        if (hit.hit && hit.panelId == pressedPanel_ &&
                            hit.controlId == pressedControl_) {
                            UiEvent e;
                            e.panelId = p.id;
                            e.controlId = c.id;
                            switch (c.kind) {
                                case UiControlKind::BUTTON:
                                    e.type = UI_EVENT_BUTTON;
                                    e.value = 1.0f;
                                    break;
                                case UiControlKind::SLIDER:
                                    e.type = UI_EVENT_SLIDER;
                                    e.value = c.value;
                                    break;
                                case UiControlKind::TOGGLE:
                                    c.value = c.value > 0.5f ? 0.0f : 1.0f;
                                    e.type = UI_EVENT_TOGGLE;
                                    e.value = c.value;
                                    break;
                                default:
                                    e.type = UI_EVENT_BUTTON;
                                    e.value = 0.0f;
                                    break;
                            }
                            outEvents.push_back(e);
                        }
                    }
                }
            }
            pressedControl_ = -1;
            pressedPanel_ = -1;
            break;
        }
        case UiPointerEvent::CANCEL: {
            for (auto& p : panels_)
                for (auto& c : p.controls) c.pressed = false;
            pressedControl_ = -1;
            pressedPanel_ = -1;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// TextAtlas
// ---------------------------------------------------------------------------
void TextAtlas::set(GLuint texId, int atlasW, int atlasH, float baseHeightPx,
                    const std::vector<GlyphInfo>& glyphs) {
    texture_ = texId;
    atlasW_ = atlasW;
    atlasH_ = atlasH;
    baseHeightPx_ = baseHeightPx > 0 ? baseHeightPx : 64.0f;
    glyphs_ = glyphs;
}

uint32_t TextAtlas::nextUtf8(const char* s, int& i) const {
    unsigned char c = static_cast<unsigned char>(s[i]);
    uint32_t cp = 0;
    if (c < 0x80) { cp = c; i += 1; }
    else if ((c & 0xE0) == 0xC0) {
        cp = (c & 0x1F);
        if (s[i + 1]) { cp = (cp << 6) | (s[i + 1] & 0x3F); i += 2; }
        else i += 1;
    } else if ((c & 0xF0) == 0xE0) {
        cp = (c & 0x0F);
        if (s[i + 1] && s[i + 2]) {
            cp = (cp << 6) | (s[i + 1] & 0x3F);
            cp = (cp << 6) | (s[i + 2] & 0x3F);
            i += 3;
        } else i += 1;
    } else {
        cp = (c & 0x07);
        if (s[i + 1] && s[i + 2] && s[i + 3]) {
            cp = (cp << 6) | (s[i + 1] & 0x3F);
            cp = (cp << 6) | (s[i + 2] & 0x3F);
            cp = (cp << 6) | (s[i + 3] & 0x3F);
            i += 4;
        } else i += 1;
    }
    return cp;
}

const GlyphInfo* TextAtlas::findGlyph(uint32_t code) const {
    // busca linear com cache implícito (glifos consecutivos ficam próximos)
    for (const auto& g : glyphs_)
        if (g.code == code) return &g;
    return nullptr;
}

float TextAtlas::measureText(const char* utf8, float heightM) const {
    float scale = heightM / baseHeightPx_;
    float total = 0.0f;
    int i = 0;
    while (utf8[i]) {
        uint32_t cp = nextUtf8(utf8, i);
        const GlyphInfo* g = findGlyph(cp);
        total += (g ? g->advance : baseHeightPx_ * 0.5f) * scale;
    }
    return total;
}

float TextAtlas::buildQuads(const char* utf8, float xMeters, float yMeters,
                            float heightMeters, const float rgba[4],
                            std::vector<float>& outQuads) const {
    float scale = heightMeters / baseHeightPx_;
    float x = xMeters;
    int i = 0;
    while (utf8[i]) {
        uint32_t cp = nextUtf8(utf8, i);
        const GlyphInfo* g = findGlyph(cp);
        if (!g || g->w <= 0.0f) {
            x += baseHeightPx_ * 0.5f * scale;
            continue;
        }
        float u0 = g->x / atlasW_;
        float v0 = g->y / atlasH_;
        float u1 = (g->x + g->w) / atlasW_;
        float v1 = (g->y + g->h) / atlasH_;
        float x0 = x + g->xoff * scale;
        float y0 = yMeters - (g->yoff + g->h) * scale; // baseline: yMeters
        float x1 = x0 + g->w * scale;
        float y1 = y0 + g->h * scale;

        // 2 triângulos: pos(2) + uv(2) + rgba(4)
        float quad[6][8] = {
            {x0, y0, u0, v0, rgba[0], rgba[1], rgba[2], rgba[3]},
            {x1, y0, u1, v0, rgba[0], rgba[1], rgba[2], rgba[3]},
            {x1, y1, u1, v1, rgba[0], rgba[1], rgba[2], rgba[3]},
            {x0, y0, u0, v0, rgba[0], rgba[1], rgba[2], rgba[3]},
            {x1, y1, u1, v1, rgba[0], rgba[1], rgba[2], rgba[3]},
            {x0, y1, u0, v1, rgba[0], rgba[1], rgba[2], rgba[3]},
        };
        for (auto& v : quad)
            for (int k = 0; k < 8; ++k) outQuads.push_back(v[k]);

        x += g->advance * scale;
    }
    return x - xMeters;
}

} // namespace brazilmr
