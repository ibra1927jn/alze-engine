#pragma once

#include "renderer/SpriteBatch2D.h"
#include "renderer/TextRenderer.h"
#include "renderer/ShapeRenderer2D.h"
#include "math/Vector2D.h"
#include "math/Color.h"
#include "math/MathUtils.h"
#include <string>

namespace engine {
namespace core {

/// UISystem — Immediate-mode UI framework.
///
/// Features:
///   - Panels, buttons, sliders, checkboxes, labels, progress bars
///   - Theme/skin system
///   - Mouse interaction (hover, click, drag)
///   - Renders through ShapeRenderer2D + TextRenderer + SpriteBatch2D
///
/// Usage:
///   ui.begin(mouseX, mouseY, mouseDown, mousePressed, spriteBatch);
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

    /// Iniciar frame — almacena estado del mouse y el batch para texto
    void begin(float mouseX, float mouseY, bool mouseDown, bool mousePressed,
               renderer::SpriteBatch2D& batch) {
        m_mouseX = mouseX;
        m_mouseY = mouseY;
        m_mouseDown = mouseDown;
        m_mousePressed = mousePressed;
        m_hotId = 0;
        m_batch = &batch;
    }

    void end() {
        if (!m_mouseDown) m_activeId = 0;
    }

    // ── Widgets ────────────────────────────────────────────────

    /// Draw a panel background with title
    void panel(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
               const std::string& title, float x, float y, float w, float h) {
        // Background y borde
        shapes.roundedRectFill(x + w * 0.5f, y + h * 0.5f, w, h, theme.cornerRadius, toShapeCol(theme.panelBg));
        shapes.rectOutline(x + w * 0.5f, y + h * 0.5f, w, h, toShapeCol(theme.panelBorder));

        // Barra de titulo
        shapes.rectFill(x + w * 0.5f, y + 12.0f, w, 24.0f, toShapeCol(theme.panelBorder));
        drawText(text, title, x + theme.padding, y + 4.0f, theme.panelTitle);

        // Linea separadora
        shapes.line(x, y + 24.0f, x + w, y + 24.0f, toShapeCol(theme.separatorColor));
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
        shapes.roundedRectFill(x + w * 0.5f, y + h * 0.5f, w, h, theme.cornerRadius, toShapeCol(bg));

        // Centrar texto
        float textW = text.measureWidth(label, 1.0f);
        float tx = x + (w - textW) * 0.5f;
        float ty = y + (h - 7.0f) * 0.5f; // 7 = altura del font
        drawText(text, label, tx, ty, theme.buttonText);

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
        float sliderW = w - labelW - 40.0f;
        bool changed = false;

        bool hover = isInside(sliderX, y, sliderW, h);
        if (hover && m_mousePressed) m_activeId = id;

        if (m_activeId == id && m_mouseDown) {
            float t = math::MathUtils::clamp(
                (m_mouseX - sliderX) / sliderW, 0.0f, 1.0f);
            *value = min + t * (max - min);
            changed = true;
        }

        // Dibujar etiqueta
        drawText(text, label, x, y + 3.0f, theme.labelColor);

        // Dibujar pista
        shapes.roundedRectFill(sliderX + sliderW * 0.5f, y + 10.0f, sliderW, 8.0f, 3.0f, toShapeCol(theme.sliderBg));

        // Dibujar relleno
        float t = math::MathUtils::clamp((*value - min) / (max - min), 0.0f, 1.0f);
        float fillW = t * sliderW;
        if (fillW > 1.0f) {
            shapes.roundedRectFill(sliderX + fillW * 0.5f, y + 10.0f, fillW, 8.0f, 3.0f, toShapeCol(theme.sliderFill));
        }

        // Dibujar knob
        float knobX = sliderX + fillW;
        shapes.circleFill(knobX, y + 10.0f, 6.0f, toShapeCol(theme.sliderKnob));

        // Texto del valor
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f", *value);
        drawText(text, buf, sliderX + sliderW + 4.0f, y + 3.0f, theme.labelColor);

        return changed;
    }

    /// Checkbox — toggles *value, returns true if changed
    bool checkbox(renderer::ShapeRenderer2D& shapes, renderer::TextRenderer& text,
                  const std::string& label, float x, float y, bool* value) {
        float boxSize = 16.0f;
        bool hover = isInside(x, y, boxSize, boxSize);
        bool changed = false;

        if (hover && m_mousePressed) {
            *value = !(*value);
            changed = true;
        }

        // Caja
        math::Color bg = *value ? theme.checkOn : theme.checkOff;
        if (hover) bg = bg.brighter(20);
        shapes.roundedRectFill(x + boxSize * 0.5f, y + boxSize * 0.5f, boxSize, boxSize, 3.0f, toShapeCol(bg));

        // Checkmark
        if (*value) {
            shapes.line(x + 3.0f, y + 8.0f, x + 7.0f, y + 12.0f, toShapeCol(math::Color::white()));
            shapes.line(x + 7.0f, y + 12.0f, x + 13.0f, y + 4.0f, toShapeCol(math::Color::white()));
        }

        // Etiqueta
        drawText(text, label, x + boxSize + 6.0f, y + 2.0f, theme.labelColor);

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

        drawText(text, label, x, y + 2.0f, theme.labelColor);

        shapes.roundedRectFill(barX + barW * 0.5f, y + h * 0.5f, barW, h, 3.0f, toShapeCol(theme.progressBg));
        float t = math::MathUtils::clamp(value / max, 0.0f, 1.0f);
        float fillW = t * barW;
        if (fillW > 1.0f) {
            shapes.roundedRectFill(barX + fillW * 0.5f, y + h * 0.5f, fillW, h, 3.0f, toShapeCol(theme.progressFill));
        }

        // Texto de porcentaje
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", static_cast<int>(t * 100));
        float tw = text.measureWidth(buf, 1.0f);
        drawText(text, buf, barX + (barW - tw) * 0.5f, y + 2.0f, theme.buttonText);
    }

    /// Text label
    void label(renderer::TextRenderer& text,
               const std::string& str, float x, float y,
               math::Color color = math::Color(180, 190, 210)) {
        drawText(text, str, x, y, color);
    }

    /// Horizontal separator line
    void separator(renderer::ShapeRenderer2D& shapes, float x, float y, float w) {
        shapes.line(x, y, x + w, y, toShapeCol(theme.separatorColor));
    }

private:
    /// Convertir math::Color (uint8 0-255) a ShapeRenderer2D::Color (float 0-1)
    static renderer::ShapeRenderer2D::Color toShapeCol(const math::Color& c) {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    }

    /// Convertir math::Color a SpriteColor para texto
    static renderer::SpriteColor toSpriteCol(const math::Color& c) {
        constexpr float k = 1.0f / 255.0f;
        return { c.r * k, c.g * k, c.b * k, c.a * k };
    }

    /// Dibujar texto usando el SpriteBatch almacenado en begin()
    void drawText(renderer::TextRenderer& text, const std::string& str,
                  float x, float y, const math::Color& color) {
        if (m_batch) {
            text.draw(*m_batch, str, x, y, 1.0f, toSpriteCol(color));
        }
    }

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
    uint32_t m_hotId = 0;        // Widget bajo el mouse
    uint32_t m_activeId = 0;     // Widget en interaccion
    renderer::SpriteBatch2D* m_batch = nullptr; // Batch para renderizar texto
};

} // namespace core
} // namespace engine
