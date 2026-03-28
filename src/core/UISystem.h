#pragma once

#include "renderer/SpriteBatch2D.h"
#include "renderer/TextRenderer.h"
#include "renderer/ShapeRenderer2D.h"
#include "math/Vector2D.h"
#include "math/Color.h"
#include "math/MathUtils.h"
#include <string>
#include <functional>
#include <vector>

namespace engine {
namespace core {

/// UISystem — Immediate-mode UI framework.
///
/// Features:
///   - Panels, buttons, sliders, checkboxes, labels, progress bars
///   - Theme/skin system
///   - Mouse interaction (hover, click, drag)
///   - Renders through ShapeRenderer2D + TextRenderer
///
/// Usage:
///   ui.begin(mouseX, mouseY, mouseDown);
///   ui.panel("Debug", 10, 10, 200, 300);
///   if (ui.button("Start", 20, 50, 160, 30)) { ... }
///   ui.slider("Speed", 20, 90, 160, &speed, 0, 100);
///   ui.end();
///
class UISystem {
public:
    // ── Theme ──────────────────────────────────────────────────

    struct Theme {
        math::Color panelBg         = math::Color(15, 15, 25, 220);
        math::Color panelBorder     = math::Color(60, 60, 90);
        math::Color panelTitle      = math::Color(120, 180, 255);
        math::Color buttonNormal    = math::Color(40, 40, 65);
        math::Color buttonHover     = math::Color(55, 55, 85);
        math::Color buttonActive    = math::Color(70, 100, 160);
        math::Color buttonText      = math::Color(220, 220, 240);
        math::Color sliderBg        = math::Color(30, 30, 50);
        math::Color sliderFill      = math::Color(60, 120, 200);
        math::Color sliderKnob      = math::Color(180, 200, 255);
        math::Color checkOn         = math::Color(60, 180, 120);
        math::Color checkOff        = math::Color(50, 50, 70);
        math::Color labelColor      = math::Color(180, 190, 210);
        math::Color progressBg      = math::Color(25, 25, 40);
        math::Color progressFill    = math::Color(60, 180, 120);
        math::Color separatorColor  = math::Color(50, 50, 70);
        float cornerRadius          = 4.0f;
        float padding               = 8.0f;
    };

    Theme theme;

    // ── Frame lifecycle ────────────────────────────────────────

    void begin(float mouseX, float mouseY, bool mouseDown, bool mousePressed) {
        m_mouseX = mouseX;
        m_mouseY = mouseY;
        m_mouseDown = mouseDown;
        m_mousePressed = mousePressed;
        m_hotId = 0;
    }

    void end() {
        if (!m_mouseDown) m_activeId = 0;
    }

    // ── Widgets ────────────────────────────────────────────────

    /// Draw a panel background with title
    void panel(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
               const std::string& title, float x, float y, float w, float h) {
        // Background
        shapes.drawRoundedRectFilled(x, y, w, h, theme.cornerRadius, theme.panelBg);
        shapes.drawRoundedRectOutline(x, y, w, h, theme.cornerRadius, theme.panelBorder);

        // Title bar
        shapes.drawRectFilled(x, y, w, 24, theme.panelBorder);
        text.drawText(title, x + theme.padding, y + 4, 1.0f, theme.panelTitle);

        // Separator line
        shapes.drawLine(x, y + 24, x + w, y + 24, theme.separatorColor);
    }

    /// Interactive button — returns true if clicked this frame
    bool button(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                const std::string& label, float x, float y, float w, float h) {
        uint32_t id = hashId(label, x, y);
        bool hover = isInside(x, y, w, h);
        bool clicked = false;

        if (hover) {
            m_hotId = id;
            if (m_mousePressed) {
                m_activeId = id;
            }
            if (m_activeId == id && !m_mouseDown) {
                clicked = true; // Released while hovering
            }
        }

        math::Color bg = (m_activeId == id && hover) ? theme.buttonActive :
                         hover ? theme.buttonHover : theme.buttonNormal;
        shapes.drawRoundedRectFilled(x, y, w, h, theme.cornerRadius, bg);

        // Center text
        float textW = text.measureWidth(label, 1.0f);
        float tx = x + (w - textW) * 0.5f;
        float ty = y + (h - 7) * 0.5f; // 7 = font height
        text.drawText(label, tx, ty, 1.0f, theme.buttonText);

        return clicked;
    }

    /// Horizontal slider — modifies *value in [min, max], returns true if changed
    bool slider(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                const std::string& label, float x, float y, float w,
                float* value, float min, float max) {
        uint32_t id = hashId(label, x, y);
        float h = 20.0f;
        float labelW = 60.0f;
        float sliderX = x + labelW;
        float sliderW = w - labelW - 40;
        bool changed = false;

        bool hover = isInside(sliderX, y, sliderW, h);
        if (hover && m_mousePressed) m_activeId = id;

        if (m_activeId == id && m_mouseDown) {
            float t = math::MathUtils::clamp(
                (m_mouseX - sliderX) / sliderW, 0.0f, 1.0f);
            *value = min + t * (max - min);
            changed = true;
        }

        // Draw label
        text.drawText(label, x, y + 3, 1.0f, theme.labelColor);

        // Draw track
        shapes.drawRoundedRectFilled(sliderX, y + 6, sliderW, 8, 3, theme.sliderBg);

        // Draw fill
        float t = math::MathUtils::clamp((*value - min) / (max - min), 0.0f, 1.0f);
        float fillW = t * sliderW;
        if (fillW > 1) {
            shapes.drawRoundedRectFilled(sliderX, y + 6, fillW, 8, 3, theme.sliderFill);
        }

        // Draw knob
        float knobX = sliderX + fillW;
        shapes.drawCircleFilled(knobX, y + 10, 6, theme.sliderKnob);

        // Value text
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", *value);
        text.drawText(buf, sliderX + sliderW + 4, y + 3, 1.0f, theme.labelColor);

        return changed;
    }

    /// Checkbox — toggles *value, returns true if changed
    bool checkbox(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                  const std::string& label, float x, float y, bool* value) {
        uint32_t id = hashId(label, x, y);
        float boxSize = 16.0f;
        bool hover = isInside(x, y, boxSize, boxSize);
        bool changed = false;

        if (hover && m_mousePressed) {
            *value = !(*value);
            changed = true;
        }

        // Box
        math::Color bg = *value ? theme.checkOn : theme.checkOff;
        if (hover) bg = bg.brighter(20);
        shapes.drawRoundedRectFilled(x, y, boxSize, boxSize, 3, bg);

        // Checkmark
        if (*value) {
            shapes.drawLine(x + 3, y + 8, x + 7, y + 12, math::Color::white());
            shapes.drawLine(x + 7, y + 12, x + 13, y + 4, math::Color::white());
        }

        // Label
        text.drawText(label, x + boxSize + 6, y + 2, 1.0f, theme.labelColor);

        return changed;
    }

    /// Progress bar (read-only)
    void progressBar(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                     const std::string& label, float x, float y, float w,
                     float value, float max = 1.0f) {
        float h = 16.0f;
        float labelW = 60.0f;
        float barX = x + labelW;
        float barW = w - labelW;

        text.drawText(label, x, y + 2, 1.0f, theme.labelColor);

        shapes.drawRoundedRectFilled(barX, y, barW, h, 3, theme.progressBg);
        float t = math::MathUtils::clamp(value / max, 0.0f, 1.0f);
        float fillW = t * barW;
        if (fillW > 1) {
            shapes.drawRoundedRectFilled(barX, y, fillW, h, 3, theme.progressFill);
        }

        // Percentage text
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(t * 100));
        float tw = text.measureWidth(buf, 1.0f);
        text.drawText(buf, barX + (barW - tw) * 0.5f, y + 2, 1.0f, theme.buttonText);
    }

    /// Text label
    void label(renderer::TextRenderer& text,
               const std::string& str, float x, float y,
               math::Color color = math::Color(180, 190, 210)) {
        text.drawText(str, x, y, 1.0f, color);
    }

    /// Horizontal separator line
    void separator(renderer::ShapeRenderer2D& shapes, float x, float y, float w) {
        shapes.drawLine(x, y, x + w, y, theme.separatorColor);
    }

private:
    bool isInside(float x, float y, float w, float h) const {
        return m_mouseX >= x && m_mouseX <= x + w &&
               m_mouseY >= y && m_mouseY <= y + h;
    }

    uint32_t hashId(const std::string& label, float x, float y) const {
        uint32_t h = 2166136261u;
        for (char c : label) h = (h ^ static_cast<uint32_t>(c)) * 16777619u;
        h ^= static_cast<uint32_t>(x * 100) * 2654435761u;
        h ^= static_cast<uint32_t>(y * 100) * 2246822519u;
        return h == 0 ? 1 : h;
    }

    float m_mouseX = 0, m_mouseY = 0;
    bool m_mouseDown = false;
    bool m_mousePressed = false; // true only on the frame mouse was pressed
    uint32_t m_hotId = 0;       // Widget under mouse
    uint32_t m_activeId = 0;    // Widget being interacted with
};

} // namespace core
} // namespace engine
